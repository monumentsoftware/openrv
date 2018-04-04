#include "socket.h"
#include <libopenrv/libopenrv.h>
#include "threadnotifier.h"
#include "orvvncclientshareddata.h"
#include "orvclientdefines.h"
#if defined(OPENRV_HAVE_MBEDTLS)
#include "mbedtlscontext.h"
#endif // OPENRV_HAVE_MBEDTLS
#if defined(OPENRV_HAVE_OPENSSL)
#include "opensslcontext.h"
#endif // OPENRV_HAVE_OPENSSL

#if !defined(_MSC_VER)
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#else // _MSC_VER
#include <WinSock2.h>
#include <Ws2tcpip.h>
#endif // _MSC_VER
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <algorithm>
#include <atomic>

#if defined(_MSC_VER)
#define ssize_t int
static bool g_time_win32_time_initialized = false;
static LARGE_INTEGER g_time_win32_performance_frequency; // # of ticks per second
#endif // _MSC_VER

#ifdef __MACH__
// osx does not implement clock_gettime().
#include <mach/mach_time.h>
#include <mutex>
static bool g_time_mach_time_initialized = false;
static double g_time_mach_timebase = 0.0;
static uint64_t g_time_mach_timestart = 0;
#endif // __MACH__

#if !defined(_MSC_VER)
inline void closesocket(int fd)
{
    ::close(fd);
}
#endif // !_MSC_VER

#define ORV_SUB_ERROR_CODE_READ_WRITE_TIMEOUT 100

namespace openrv {

static uint64_t getTimestampUs();
static void makeTimeout(uint64_t* timeoutSec, uint64_t* timeoutUsec, uint64_t lastActivityTimeUs, uint64_t currentTimeUs, uint64_t timeoutUs);
static int getLastErrorCode();

/**
 * @param pipeListener The @ref ThreadNotifierListener object that provides a pipe fd that is used
 *        by external threads to interrupt socket operations.
 * @param communicationData Additional data shared between this and external threads. This most
 *        notably holds the "want abort" boolean flag.
 *        The pointer must remain valid for the lifetime of this object.
 **/
Socket::Socket(orv_context_t* ctx, ThreadNotifierListener* pipeListener, vnc::OrvVncClientSharedData* communicationData)
    : mContext(ctx),
      mPipeListener(pipeListener)
{
    mCommunicationDataMutex = &communicationData->mMutex;
    mCommunicationDataUserRequestedDisconnect = &communicationData->mUserRequestedDisconnect;
}

/**
 * @overload
 **/
Socket::Socket(orv_context_t* ctx, ThreadNotifierListener* pipeListener, std::mutex* sharedDataMutex, bool* sharedDataUserRequestedDisconnect)
    : mContext(ctx),
      mPipeListener(pipeListener),
      mCommunicationDataMutex(sharedDataMutex),
      mCommunicationDataUserRequestedDisconnect(sharedDataUserRequestedDisconnect)
{
}

Socket::~Socket()
{
    close();
}

/**
 * Request and initialize a socket, i.e. call @ref ::socket() and set initial options. Initial
 * options most notably include
 * - timeout, see @ref timeoutSeconds()
 * - disable Nagle's algorithm
 *
 * NOTE: This function is normally not used directly, but internally by @ref
 * makeSocketAndConnectBlockingTo(), because it requires the address/protocol family (which is
 * obtained from the hostname) first.
 *
 * @param addressFamily Whether to use ipv4 (AF_INET) or ipv6 (AF_INET6).
 *        Note that PF_INET and PF_INET6 are in practice identical to AF_INET and AF_INET6,
 *        so the value ai_info from getaddrinfo() can be used directly.
 * @return TRUE on success, FALSE on error. If the socket was already open when this function was
 *         called, this function fails immediately (socket remains open and no new socket is
 *         opened).
 **/
bool Socket::makeSocket(int addressFamily, orv_error_t* error)
{
    if (isOpened()) {
        orv_error_set(error, 0, ORV_ERR_CONNECT_ERROR_GENERIC, "Internal error: Unable to open new socket, another socket already opened");
        return false;
    }

#if defined(_MSC_VER)
    unsigned short ver = MAKEWORD(2, 2);
    WSADATA wsaData = { 0 };
    int wsaStartupRet = WSAStartup(ver, &wsaData);
    if (wsaStartupRet != 0) {
        orv_error_set(error, 0, ORV_ERR_CONNECT_ERROR_GENERIC, "Failed to initialize windows socket library");
        return false;
    }
#endif

    resetStatistics();
    mSocketFd = ::socket(addressFamily, SOCK_STREAM, 0);
    if (mSocketFd == -1) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Failed to create socket");
        return false;
    }

    // Make socket non-blocking
#if !defined(_MSC_VER)
    {
        int flags = fcntl(mSocketFd, F_GETFL, 0);
        if (flags == -1) {
            int lastError = getLastErrorCode();
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 1, "Failed to obtain socket flags, errno=%d", lastError);
            closesocket(mSocketFd);
            mSocketFd = -1;
            return false;
        }
        flags |= O_NONBLOCK;
        if (fcntl(mSocketFd, F_SETFL, flags) == -1) {
            int lastError = getLastErrorCode();
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 1, "Failed to make socket non-blocking, errno=%d", lastError);
            closesocket(mSocketFd);
            mSocketFd = -1;
            return false;
        }
    }
#else // !_MSC_VER
    {
        u_long arg = 1;
        if (ioctlsocket(mSocketFd, FIONBIO, &arg) != 0) {
            int lastError = getLastErrorCode();
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 1, "Failed to make socket non-blocking, WSAGetLastError=%d", lastError);
            closesocket(mSocketFd);
            mSocketFd = -1;
            return false;
        }
    }
#endif // !_MSC_VER

    // Disable Nagle's algorithm.
    // By default, data is queued by the OS and actual send is delayed until a certain condition is
    // met (enough data is in queue, ACK is received, timeout is reached, etc.), however this is
    // very bad for VNC, where most messages are small (i.e. rarely fill a full TCP packet), are
    // time-critical and normally no more data is written until the other side responds. So Nagle's
    // algorithm is useless to us.
    // NOTE: This implies that send(sock, buffer, 1, flags); send(sock, buffer, 1, flags); are very
    //       bad. Instead only a single send() should be used.
    int flag = 1;
    if (setsockopt(mSocketFd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int)) < 0) { // NOTE: win32 takes char* instead of void*
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 1, "Failed to disable Nagle's algorithm on socket");
        closesocket(mSocketFd);
        mSocketFd = -1;
        return false;
    }

#ifdef SO_NOSIGPIPE
    // Do not send SIGPIPE on send() if socket was closed by remote
    flag = 1;
    if (setsockopt(mSocketFd, SOL_SOCKET, SO_NOSIGPIPE, &flag, sizeof(int)) < 0) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 1, "Failed to disable SIGPIPE on socket");
        closesocket(mSocketFd);
        mSocketFd = -1;
        return false;
    }
#else // SO_NOSIGPIPE
    // Apple systems use #define to define SO_NOSIGPIPE, so we can easily #ifdef for them.
    // This check here merely exists in case this is different in some variants of apple systems, so
    // we get an error immediately.
#ifdef __APPLE__
#error Apple system detected, but SO_NOSIGPIPE not defined. Check this code!
#endif // __APPLE__
#endif // SO_NOSIGPIPE

#ifdef _MSC_VER
    mSocketEvent = WSACreateEvent();
    if (mSocketEvent == nullptr) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 1, "Failed to create event object for socket fd");
        closesocket(mSocketFd);
        mSocketFd = -1;
        return false;
    }
#endif // _MSC_VER

    return true;
}

/**
 * Close the socket. If the socket is already closed, this function does nothing.
 **/
void Socket::close()
{
    if (mSocketFd != -1) {
        closesocket(mSocketFd); // NOTE: We ignore any errors here. We can't do anything useful about it. In particular: may return error if socket already closed
        mSocketFd = -1;
#ifdef _MSC_VER
        if (mSocketEvent) {
            WSACloseEvent((WSAEVENT)mSocketEvent);
            mSocketEvent = nullptr;
        }
#endif // _MSC_VER
    }
    mOpenSSLContext = nullptr;
    mMbedTlsContext = nullptr;
}

/**
 * Set the socket timeout to @p timeoutSeconds.
 *
 * If the socket is already open (connected or otherwise), the timeout is applied to the socket
 * immediately, otherwise the value will merely be stored internally and set on open.
 *
 **/
void Socket::setSocketTimeoutSeconds(int timeoutSeconds)
{
    // NOTE: read/write timeouts are activity timeouts: they are reset whenever at least one byte is
    //       received/sent.
    mSocketTimeoutSeconds = timeoutSeconds;
}

/**
 * Call ::send() on the internal socket until either @p nbyte have been sent or an error occurred.
 * This function counts the number of sent bytes internally.
 *
 * @return TRUE on success (@p nbyte bytes have been sent), FALSE on failure. On failure, the reason
 *         for failure is stored in @p error.
 *         This function may fail if
 *         - remote closed the connection (@ref ORV_ERR_CLOSED_BY_REMOTE)
 *         - user closed requested to close/abort the connection (@ref ORV_ERR_USER_INTERRUPTION)
 *         - failed to send data to socket (@ref ORV_ERR_WRITE_FAILED)
 **/
bool Socket::writeDataBlocking(const void* buf, size_t nbyte, orv_error_t* error)
{
    uint64_t lastActivityTimeUs = 0;
    size_t sentBytes = 0;
    while (true) {
        const size_t remainingBytes = nbyte - sentBytes;
        ssize_t s = 0;
        int lastErrorForSend = 0;
        SendRecvSocketError sendError = sendData(((char*)buf) + sentBytes, remainingBytes, &s, &lastErrorForSend);
        switch (sendError) {
            case SendRecvSocketError::NoError:
                break;
            case SendRecvSocketError::CallAgainWaitForWrite:
                // select() to write more data
                break;
            case SendRecvSocketError::CallAgainWaitForRead:
                // select() on read (!) to write more data
                break;
            case SendRecvSocketError::ClosedByRemote:
                orv_error_set(error, ORV_ERR_CLOSED_BY_REMOTE, 0, "Failed to write/send %u bytes to socket (only %u bytes sent), connection closed by peer.", (unsigned int)nbyte, (unsigned int)sentBytes);
                return false;
            case SendRecvSocketError::ResetByRemote:
                orv_error_set(error, ORV_ERR_CLOSED_BY_REMOTE, 0, "Failed to write/send %u bytes to socket (only %u bytes sent), connection reset by peer.", (unsigned int)nbyte, (unsigned int)sentBytes);
                return false;
            default:
            case SendRecvSocketError::GenericError:
#if !defined(_MSC_VER)
                orv_error_set(error, ORV_ERR_WRITE_FAILED, 0, "Failed to write/send %u bytes to socket (only %u bytes sent), send failed with errno=%d.", (unsigned int)nbyte, (unsigned int)sentBytes, lastErrorForSend);
#else // !_MSC_VER
                orv_error_set(error, ORV_ERR_WRITE_FAILED, 0, "Failed to write/send %u bytes to socket (only %u bytes sent), send failed with WSAGetLastError()=%d.", (unsigned int)nbyte, (unsigned int)sentBytes, lastErrorForSend);
#endif // !_MSC_VER)
                return false;
            case SendRecvSocketError::InternalErrorUnreachableCode:
                orv_error_set(error, ORV_ERR_WRITE_FAILED, 0, "Internal error while trying to send to socket. Reached code that should be unreachable.");
                return false;
        }
        if (s > 0) {
            mSentBytes += s;
            sentBytes += s;
        }
        if (sentBytes == nbyte) {
            return true;
        }
        if (s > 0 || lastActivityTimeUs == 0) {
            lastActivityTimeUs = getTimestampUs();
        }

        uint64_t currentTimeUs = lastActivityTimeUs; // if s>0, no timeout possible (timeout==0 not supported)
        if (s <= 0) {
            uint64_t currentTimeUs = getTimestampUs();
            uint64_t timeDiffUs = (currentTimeUs - lastActivityTimeUs);
            if (timeDiffUs > socketWriteTimeoutUs()) {
                orv_error_set(error, ORV_ERR_WRITE_FAILED, ORV_SUB_ERROR_CODE_READ_WRITE_TIMEOUT, "Timeout trying to write %u bytes to socket, only %u bytes sent so far.", (unsigned int)nbyte, (unsigned int)sentBytes);
                return false;
            }
        }

        uint64_t timeoutSec = 0;
        uint64_t timeoutUsec = 0;
        makeTimeout(&timeoutSec, &timeoutUsec, lastActivityTimeUs, currentTimeUs, socketWriteTimeoutUs());

        int lastError = 0;
        WaitType waitType = WaitType::Write;
        if (sendError == SendRecvSocketError::CallAgainWaitForRead) {
            // NOTE: On SSL connection we may actually need to wait for *read* when we want to write
            //       (e.g. when SSL_write() needs to renegotiate)
            waitType = WaitType::Read;
            ORV_DEBUG(mContext, "Need to wait for socket being readable for SSL_write()");
        }
        WaitRet waitRet = waitForSignal(timeoutSec, timeoutUsec, true, waitType, &lastError);
        switch (waitRet) {
            case WaitRet::UserInterruption:
                orv_error_set(error, ORV_ERR_USER_INTERRUPTION, 0, ORV_ERROR_MSG_USER_INTERRUPTION);
                return false;
            case WaitRet::Error:
                orv_error_set(error, ORV_ERR_WRITE_FAILED, 0, "Failed to write/send %u bytes to socket (only %u bytes sent), lastError=%d.", (unsigned int)nbyte, (unsigned int)sentBytes, lastError);
                return false;
            case WaitRet::Signalled:
                break;
            case WaitRet::Timeout:
                // nothing to do (handled on next iteration prior to calling select())
                break;
        }
    }
    orv_error_set(error, ORV_ERR_WRITE_FAILED, 0, "Internal error while trying to send to socket. Reached code that should be unreachable.");
    return false;
}

/**
 * Call ::recv() on the internal socket until either @b nbyte have been received or an error
 * occurred. This function counts the received bytes internally.
 *
 * @return TRUE on success (@p nbyte bytes have been read), FALSE on failure. On failure, the reason
 *         for failure is stored in @p error.
 *         This function may fail if
 *         - remote closed the connection (@ref ORV_ERR_CLOSED_BY_REMOTE)
 *         - user closed requested to close/abort the connection (@ref ORV_ERR_USER_INTERRUPTION)
 *         - failed to read data from socket (@ref ORV_ERR_READ_FAILED)
 **/
bool Socket::readDataBlocking(void* buf, size_t nbyte, orv_error_t* error)
{
    uint64_t lastActivityTimeUs = 0;
    size_t readBytes = 0;
    while (true) {
        const size_t remainingBytes = nbyte - readBytes;
        ssize_t s = 0;
        int lastErrorForReceive = 0;
        SendRecvSocketError recvError = receiveData(((char*)buf) + readBytes, remainingBytes, &s, &lastErrorForReceive);
        switch (recvError) {
            case SendRecvSocketError::NoError:
                break;
            case SendRecvSocketError::CallAgainWaitForWrite:
                // select() on write (!) to read  more data
                break;
            case SendRecvSocketError::CallAgainWaitForRead:
                // select() to read  more data
                break;
            case SendRecvSocketError::ClosedByRemote:
                orv_error_set(error, ORV_ERR_CLOSED_BY_REMOTE, 0, "Failed to read %u bytes from socket (only %u bytes read), remote closed the connection.", (unsigned int)nbyte, (unsigned int)readBytes);
                return false;
            case SendRecvSocketError::ResetByRemote:
                orv_error_set(error, ORV_ERR_CLOSED_BY_REMOTE, 0, "Failed to read %u bytes from socket (only %u bytes read), connection reset by peer.", (unsigned int)nbyte, (unsigned int)readBytes);
                return false;
            case SendRecvSocketError::InternalErrorUnreachableCode:
                orv_error_set(error, ORV_ERR_READ_FAILED, 0, "Internal error while reading data from socket, encountered code that should be unreachable, errno/WSAGetLastError()=%d", lastErrorForReceive);
                return false;
            default:
            case SendRecvSocketError::GenericError:
#if !defined(_MSC_VER)
                orv_error_set(error, ORV_ERR_READ_FAILED, 0, "Failed to read %u bytes from socket (only %u bytes read), recv failed with errno=%d.", (unsigned int)nbyte, (unsigned int)readBytes, (int)lastErrorForReceive);
#else // _MSC_VER
                orv_error_set(error, ORV_ERR_READ_FAILED, 0, "Failed to read %u bytes from socket (only %u bytes read), recv failed with WSAGetLastError()=%d.", (unsigned int)nbyte, (unsigned int)readBytes, (int)lastErrorForReceive);
#endif // MSG_NOSIGNAL
                return false;
        }
        if (s > 0) {
            mReceivedBytes += s;
            readBytes += s;
        }
        if (readBytes == nbyte) {
            return true;
        }
        if (s > 0 || lastActivityTimeUs == 0) {
            lastActivityTimeUs = getTimestampUs();
        }

        uint64_t currentTimeUs = lastActivityTimeUs; // if s>0, no timeout possible (timeout==0 not supported)
        if (s <= 0) {
            uint64_t currentTimeUs = getTimestampUs();
            uint64_t timeDiffUs = (currentTimeUs - lastActivityTimeUs);
            if (timeDiffUs > socketReadTimeoutUs()) {
                orv_error_set(error, ORV_ERR_READ_FAILED, ORV_SUB_ERROR_CODE_READ_WRITE_TIMEOUT, "Timeout trying to read %u bytes from socket, only %u bytes read so far.", (unsigned int)nbyte, (unsigned int)readBytes);
                return false;
            }
        }

        uint64_t timeoutSec = 0;
        uint64_t timeoutUsec = 0;
        makeTimeout(&timeoutSec, &timeoutUsec, lastActivityTimeUs, currentTimeUs, socketReadTimeoutUs());

        int lastError = 0;
        WaitType waitType = WaitType::Read;
        if (recvError == SendRecvSocketError::CallAgainWaitForWrite) {
            // NOTE: On SSL connection we may actually need to wait for *write* when we want to read
            //       (e.g. when SSL_read() needs to renegotiate)
            waitType = WaitType::Write;
            ORV_DEBUG(mContext, "Need to wait for socket being writable for SSL_read()");
        }
        WaitRet waitRet = waitForSignal(timeoutSec, timeoutUsec, true, waitType, &lastError);
        switch (waitRet) {
            case WaitRet::UserInterruption:
                orv_error_set(error, ORV_ERR_USER_INTERRUPTION, 0, ORV_ERROR_MSG_USER_INTERRUPTION);
                return false;
            case WaitRet::Error:
                orv_error_set(error, ORV_ERR_READ_FAILED, 0, "Failed to read %u bytes from socket (only %u bytes read),failed with lastError=%d.", (unsigned int)nbyte, (unsigned int)readBytes, lastError);
                return false;
            case WaitRet::Signalled:
                break;
            case WaitRet::Timeout:
                // nothing to do (handled on next iteration prior to calling select())
                break;
        }
    }
    orv_error_set(error, ORV_ERR_READ_FAILED, 0, "Internal error while trying to read from socket. Reached code that should be unreachable.");
    return false;
}

/**
 * Call ::recv() on the internal socket to read all available data or up to @p nbyte bytes
 * (whichever is smaller). This function does @em not block, if no data is available, this function
 * succeeds and returns 0 (caller is expected to select() on the socket until more data is
 * available).
 *
 * WARNING subtle caveat: Even though this function attempts to @em read data, it may in principle
 *         require the caller to wait for a socket to be @em writable in the next select() call,
 *         which is indicated by returning 0 (indicating to call select()) @em AND setting @p
 *         callAgainType to @ref SendRecvSocketError::CallAgainWaitForWrite (indicating that
 *         select() should wait for socket to be writable).
 *         This may happen if the current connection is SSL encrypted and in order to read more
 *         data, an SSL renegotiation is required (see documentation of OpenSSL's SSL_read()).
 *
 * This function counts the received bytes internally.
 *
 *
 * @param callAgainType If this function returns 0 @em and @p error has no error, then this
 *        parameter holds @ref SendRecvSocketError::CallAgainWaitForRead if the caller should
 *        select() for the socket to be readable and @ref SendRecvSocketError::CallAgainWaitForWrite
 *        if the caller should select for the socket to be writable.
 *        Normally, this function always requires a select() on read,
 *        however in certain cases on encrypted connections, the caller may actually need to
 *        select() for the socket being @em writable before calling this function again (if SSL
 *        renegotiation is required).
 * @return The number of bytes read from the socket on success, or 0 on error. On error @p error is
 *         set accordingly.
 *         This function may fail if
 *         - remote closed the connection (@ref ORV_ERR_CLOSED_BY_REMOTE)
 *         - failed to read data from socket (@ref ORV_ERR_READ_FAILED)
 *         note that contrary to @ref readDataBlocking(), this function can not time out.
 **/
uint32_t Socket::readAvailableDataNonBlocking(void* buf, size_t nbyte, SendRecvSocketError* callAgainType, orv_error_t* error)
{
    orv_error_reset_minimal(error); // in case error was not reset on entry
    ssize_t s = 0;
    int lastErrorForReceive = 0;
    SendRecvSocketError recvError = receiveData((char*)buf, nbyte, &s, &lastErrorForReceive);
    *callAgainType = SendRecvSocketError::CallAgainWaitForRead; // fallback, value is only relevant if return value is 0 AND error->mHasError==false
    switch (recvError) {
        case SendRecvSocketError::ClosedByRemote:
            orv_error_set(error, ORV_ERR_CLOSED_BY_REMOTE, 0, "Failed to read data from socket, remote closed the connection.");
            return 0;
        case SendRecvSocketError::ResetByRemote:
            orv_error_set(error, ORV_ERR_CLOSED_BY_REMOTE, 0, "Failed to read data from socket, connection reset by peer.");
            return 0;
        case SendRecvSocketError::CallAgainWaitForRead:
        case SendRecvSocketError::CallAgainWaitForWrite:
            *callAgainType = recvError;
            // select() for more data
            // WARNING: May require waiting for *write* if connection is encrypted (due to potential
            //          SSL renegotiation)
            return 0;
        default:
#if !defined(_MSC_VER)
            orv_error_set(error, ORV_ERR_READ_FAILED, 0, "Failed to read data from socket, recv failed with errno=%d.", lastErrorForReceive);
#else // _MSC_VER
            orv_error_set(error, ORV_ERR_READ_FAILED, 0, "Failed to read data from socket, recv failed with WSAGetLastError()=%d.", lastErrorForReceive);
#endif // _MSC_VER
            return 0;
        case SendRecvSocketError::InternalErrorUnreachableCode:
            orv_error_set(error, ORV_ERR_READ_FAILED, 0, "Internal error while reading data from socket, encountered code that should be unreachable, errno/WSAGetLastError()=%d", lastErrorForReceive);
            return 0;
        case SendRecvSocketError::NoError:
            if (s > 0) {
                mReceivedBytes += s;
                return (uint32_t)s;
            }
            return 0;
    }
    orv_error_set(error, ORV_ERR_READ_FAILED, 0, "Internal error while reading data from socket, encountered code that should be unreachable, errno/WSAGetLastError()=%d", lastErrorForReceive);
    return 0;
}

/**
 * Connect to the remote server and block until the connect has finished, either successfully or
 * with failure. This function assumes the internal socket was not yet created.
 *
 * This function uses non-blocking sockets internally and select() to wait for the results. In
 * addition, the select() also listens on @ref ThreadNotifierListener which is used to abort a
 * connection (see @ref orv_disconnect()).
 *
 * @return TRUE if the connection has been established, otherwise FALSE. If this function returns
 *         FALSE, @p error will hold the reason for failure.
 **/
bool Socket::makeSocketAndConnectBlockingTo(const char* hostName, uint16_t port, orv_error_t* error)
{
    if (isOpened()) {
        orv_error_set(error, 0, ORV_ERR_CONNECT_ERROR_GENERIC, "Internal error: Unable to open new socket, another socket already opened");
        return false;
    }

    char portString[11] = {};
    snprintf(portString, 10, "%d", (int)port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = 0; // FIXME:  AI_DEFAULT is not available on windows, is 0 sufficient?
    struct addrinfo* firstInfo = nullptr;
    int ret = getaddrinfo(hostName, portString, &hints, &firstInfo);
    if (ret != 0 || !firstInfo) {
        if (firstInfo) {
            freeaddrinfo(firstInfo);
        }
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_NO_SUCH_HOST, 0, "No such host: '%s'", hostName);
        return false;
    }

    struct sockaddr_storage serverAddr;
    size_t serverAddrLen = 0;
    for (struct addrinfo* addr = firstInfo; addr; addr = addr->ai_next) {
        if (addr->ai_family != AF_INET && addr->ai_family != AF_INET6) {
            ORV_DEBUG(mContext, "Ignoring addrinfo with unexpected ai_family %x", (int)addr->ai_family);
            continue;
        }
        // use first suitable entry.
        memcpy(&serverAddr, addr->ai_addr, addr->ai_addrlen);
        serverAddrLen = addr->ai_addrlen;
        break;
    }
    if (firstInfo) {
        freeaddrinfo(firstInfo);
    }

    if (serverAddrLen <= 0) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_NO_SUCH_HOST, 0, "No such host: '%s' (no supported address family found for host)", hostName);
        return false;
    }

    if (!makeSocket(serverAddr.ss_family, error)) {
        ORV_ERROR(mContext, "Failed to create socket.");
        return false;
    }

    if (connect(mSocketFd, (const sockaddr*)&serverAddr, serverAddrLen) == 0) {
        // call succeeded immediately
        // NOTE: this rarely happens, normally we have EINPROGRESS for non-blocking socets
        return true;
    }

    {
        const int lastError = getLastErrorCode();
        bool errorOk = false;
        if (lastError == EINPROGRESS) {
            errorOk = true;
        }
#if defined(_MSC_VER)
        if (lastError == WSAEWOULDBLOCK) {
            errorOk = true;
        }
#endif
        if (!errorOk) {
            makeConnectError(error, hostName, port, lastError);
            return false;
        }
    }

    const uint64_t startTimeUs = getTimestampUs();
    uint64_t currentTimeUs = 0;
    while (true) {
        if (currentTimeUs == 0) {
            currentTimeUs = startTimeUs;
        }
        else {
            currentTimeUs = getTimestampUs();
            uint64_t timeDiffUs = (currentTimeUs - startTimeUs);
            if (timeDiffUs > socketConnectTimeoutUs()) {
                orv_error_set(error, ORV_ERR_CONNECT_ERROR_TIMEOUT, 0, "Connection to %s:%d failed, connect timeout", hostName, (int)port);
                return false;
            }
        }
        // NOTE: On linux socket is flagged in writefds when connect() finishes (either with
        //       error/failure or with success) and we are expected to use getsockopt() with
        //       option=SO_ERROR at level SOL_SOCKET to test for success/failure.
        //       On windows socket is flagged in writefds if connect() succeeds, in errorfds if
        //       connect() fails.
        //       -> however getsockopt() is still possible and in fact required to get the
        //          errorcode.

        uint64_t timeoutSec = 0;
        uint64_t timeoutUsec = 0;
        makeTimeout(&timeoutSec, &timeoutUsec, startTimeUs, currentTimeUs, socketConnectTimeoutUs());


        int lastError = 0;
        bool signalledSocket = false;
        WaitRet waitRet = waitForSignal(timeoutSec, timeoutUsec, true, WaitType::Connect, &lastError, &signalledSocket);
        switch (waitRet) {
            case WaitRet::UserInterruption:
                orv_error_set(error, ORV_ERR_USER_INTERRUPTION, 0, ORV_ERROR_MSG_USER_INTERRUPTION);
                return false;
            case WaitRet::Error:
                makeConnectSelectError(error, hostName, port, lastError);
                return false;
            case WaitRet::Signalled:
            {
                if (!signalledSocket) {
                    // Pipe was signalled, not socket.
                    // This is not a problem: The pipe data is swallowed by waitForSignal(), so we
                    // don't have a busy loop here.
                    // We will return to the main event loop once the connect is finihsed anyway,
                    // so the pipe signal won't be ignored either.
                    // We have nothing to do though: User did not request UserInterruption and
                    // socket is not connected yet, waking the thread at this point externally makes
                    // little sense.
                    break;
                }
                int socketError = 0;
#if !defined(_MSC_VER)
                socklen_t optLen = sizeof(int);
                int ret = getsockopt(mSocketFd, SOL_SOCKET, SO_ERROR, &socketError, &optLen);
#else
                int optLen = sizeof(int);
                int ret = getsockopt(mSocketFd, SOL_SOCKET, SO_ERROR, (char*)&socketError, &optLen);
#endif
                if (!signalledSocket) {
                    continue;
                }
                if (ret != 0) {
                    // TODO: abort with generic error?
                    int lastError = getLastErrorCode();
                    orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Failed to obtain connect result, getsockopt returned error, lastError=%d", (int)lastError);
                    return false;
                }
                if (socketError == 0) {
                    // no error, connection successful
                    return true;
                }
                makeConnectError(error, hostName, port, socketError);
                return false;
            }
            case WaitRet::Timeout:
                // nothing to do (handled on next iteration prior to calling select())
                break;
            }
    }
    return true;
}

void Socket::resetStatistics()
{
    mReceivedBytes = 0;
    mSentBytes = 0;
}

/**
 * @param errorCode The error code to make the error for, either errno (on unix) or
 *        WSAGetLastError() (on windows).
 **/
void Socket::makeConnectError(orv_error_t* error, const char* hostName, uint16_t port, int errorCode)
{
    switch (errorCode) {
#if !defined(_MSC_VER)
        case ECONNREFUSED:
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_CONNECTION_REFUSED, 0, "Connection to %s:%d failed, connection refused", hostName, (int)port);
            break;
        case EHOSTUNREACH:
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_HOST_UNREACHABLE, 0, "Connection to %s:%d failed, host unreachable", hostName, (int)port);
            break;
        case ETIMEDOUT:
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_TIMEOUT, 0, "Connection to %s:%d failed, connect timeout", hostName, (int)port);
            break;
        default:
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Connection to %s:%d failed, errno=%d", hostName, (int)port, errorCode);
            break;
#else // _MSC_VER
        default:
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Connection to %s:%d failed, WSAGetLastError()=%d", hostName, (int)port, errorCode);
            break;
        case WSAEHOSTUNREACH:
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_HOST_UNREACHABLE, 0, "Connection to %s:%d failed, host unreachable", hostName, (int)port);
            break;
        case WSAECONNREFUSED:
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_CONNECTION_REFUSED, 0, "Connection to %s:%d failed, connection refused", hostName, (int)port);
            break;
        case WSAETIMEDOUT:
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_TIMEOUT, 0, "Connection to %s:%d failed, connect timeout", hostName, (int)port);
            break;
#endif // _MSC_VER
    }
}

/**
 * @param errorCode The error code to make the error for, either errno (on unix) or
 *        WSAGetLastError() (on windows).
 **/
void Socket::makeConnectSelectError(orv_error_t* error, const char* hostName, uint16_t port, int errorCode)
{
#if !defined(_MSC_VER)
    orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Connection to %s:%d failed, select() failed with errno=%d", hostName, (int)port, errorCode);
#else // _MSC_VER
    orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Connection to %s:%d failed, select() failed with WSAGetLastError()=%d", hostName, (int)port, errorCode);
#endif // _MSC_VER
}

/**
 * Helper function that waits until either some signal occurs on the socket (e.g. writable or
 * readable), or the @ref ThreadNotifierListener is signalled.
 *
 * On unix this function performs a select() on the socket fd and the pipe. On windws
 * WSAWaitForMultipleEvents() is used instead.
 *
 * This function is normally used internally by this class whenever data should be read/written
 * in a blocking way, but may also be called externally. As a convenience, the @p waitType may
 * also indicate that the socket should not be considered for the wait, then this function waits
 * for the pipe only.
 **/
Socket::WaitRet Socket::waitForSignal(uint64_t timeoutSec, uint64_t timeoutUsec, bool useTimeout, WaitType waitType, int* lastError, bool* signalledSocket, bool* signalledPipe)
{
    if (signalledSocket) {
        *signalledSocket = false;
    }
    if (signalledPipe) {
        *signalledPipe = false;
    }
#ifndef _MSC_VER
    fd_set readfds;
    fd_set writefds;
    fd_set* errorfds = nullptr;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    if (mSocketFd != -1) {
        switch (waitType) {
            case WaitType::Write:
                FD_SET(mSocketFd, &writefds);
                break;
            case WaitType::Read:
                FD_SET(mSocketFd, &readfds);
                break;
            case WaitType::Connect:
                FD_SET(mSocketFd, &writefds);
                break;
            case WaitType::NoSocketWait:
                // Wait on pipe only
                break;
        }
    }
    FD_SET(mPipeListener->pipeReadFd(), &readfds);
    int nfds = std::max(mPipeListener->pipeReadFd(), mSocketFd) + 1;

    struct timeval timeout;
    struct timeval* timeoutPtr = nullptr;
    if (useTimeout) {
        timeout.tv_sec = timeoutSec;
        timeout.tv_usec = timeoutUsec;
        timeoutPtr = &timeout;
    }
    int ret = select(nfds, &readfds, &writefds, errorfds, timeoutPtr);
    if (ret < 0) {
        *lastError = getLastErrorCode();
        return WaitRet::Error;
    }
    else if (ret == 0) {
        return WaitRet::Timeout;
    }
    else {
        if (mPipeListener->pipeReadFd() > -1 && FD_ISSET(mPipeListener->pipeReadFd(), &readfds)) {
            mPipeListener->swallowPipeData();
            if (signalledPipe) {
                *signalledPipe = true;
            }
        }
        if (signalledSocket) {
            switch (waitType) {
                case WaitType::Write:
                case WaitType::Connect:
                    if (FD_ISSET(mSocketFd, &writefds)) {
                        *signalledSocket = true;
                    }
                    break;
                case WaitType::Read:
                    if (FD_ISSET(mSocketFd, &readfds)) {
                        *signalledSocket = true;
                    }
                    break;
                case WaitType::NoSocketWait:
                    break;
            }
        }
        bool wantAbort = false;
        mCommunicationDataMutex->lock();
        wantAbort = *mCommunicationDataUserRequestedDisconnect;
        mCommunicationDataMutex->unlock();
        if (wantAbort) {
            return WaitRet::UserInterruption;
        }
        return WaitRet::Signalled;
    }
#else // _MSC_VER
    long networkEvents = FD_CLOSE;
    if (mSocketFd != -1) {
        switch (waitType) {
            case WaitType::Write:
                networkEvents |= FD_WRITE;
                break;
            case WaitType::Read:
                networkEvents |= FD_READ;
                break;
            case WaitType::Connect:
                networkEvents |= FD_CONNECT;
                break;
            case WaitType::NoSocketWait:
                // wait on pipe only
                break;
        }
        if (waitType != WaitType::NoSocketWait) {
            if (WSAEventSelect(mSocketFd, mSocketEvent, networkEvents) != 0) {
                *lastError = getLastErrorCode();
                return WaitRet::Error;
            }
        }
    }
    else {
        waitType = WaitType::NoSocketWait;
    }
    const int maxEventsCount = 2;
    const int eventsCount = waitType == WaitType::NoSocketWait ? 1 : 2;
    WSAEVENT events[maxEventsCount];
    events[0] = mPipeListener->pipeReadHandle();
    events[1] = mSocketEvent;
    const int socketIndex = 1;
    DWORD timeout = 0;
    if (useTimeout) {
        timeout = (DWORD)(timeoutSec * 1000 + timeoutUsec / 1000);
    }
    else {
        timeout = WSA_INFINITE;
    }
    int ret = WSAWaitForMultipleEvents(eventsCount, events, FALSE, timeout, FALSE);
    if (ret == WSA_WAIT_FAILED) {
        *lastError = getLastErrorCode();
        return WaitRet::Error;
    }
    else if (ret == WSA_WAIT_TIMEOUT) {
        return WaitRet::Timeout;
    }
    else {
        const int firstIndex = ret - WSA_WAIT_EVENT_0;
        if (firstIndex == 0) {
            mPipeListener->swallowPipeData();
            if (signalledPipe) {
                *signalledPipe = true;
            }
        }
        // Check if socket is signalled
        if (signalledSocket) {
            if (firstIndex <= socketIndex && mSocketFd != -1 && waitType != WaitType::NoSocketWait) {
                WSANETWORKEVENTS e;
                WSAEnumNetworkEvents(mSocketFd, mSocketEvent, &e);
                if (e.lNetworkEvents != 0) {
                    *signalledSocket = true;
                }
            }
        }
        bool wantAbort = false;
        mCommunicationDatamMutex->lock();
        wantAbort = *mCommunicationDataUserRequestedDisconnect;
        mCommunicationDatamMutex->unlock();
        if (wantAbort) {
            return WaitRet::UserInterruption;
        }
        return WaitRet::Signalled;
    }
#endif // _MSC_VER}
}

/**
 * Set the encryption context to @p mbedTlsContext. From this point on, @p mbedTlsContext will be
 * used to send or receive data.
 *
 * The provided pointer must remain valid for the lifetime of this object or until another context
 * (possibly a NULL context) is set. This object does NOT take ownership of the provided pointer.
 **/
void Socket::setEncryptionContext(MbedTlsContext* mbedTlsContext)
{
    mMbedTlsContext = mbedTlsContext;
    mOpenSSLContext = nullptr;

#if !defined(OPENRV_HAVE_MBEDTLS)
    if (mMbedTlsContext) {
        // should never happen, the MbedTlsContext class should not exist
        mMbedTlsContext = nullptr;
        ORV_ERROR(mContext, "Tried to set MbedTlsContext context, but library is built without mbedtls support. Rejecting context.");
    }
#endif // OPENRV_HAVE_MBEDTLS
}

/**
 * Set the encryption context to @p openSSLContext. From this point on, @p openSSLContext will be
 * used to send or receive data.
 *
 * The provided pointer must remain valid for the lifetime of this object or until another context
 * (possibly a NULL context) is set. This object does NOT take ownership of the provided pointer.
 **/
void Socket::setEncryptionContext(OpenSSLContext* openSSLContext)
{
    mMbedTlsContext = nullptr;
    mOpenSSLContext = openSSLContext;

#if !defined(OPENRV_HAVE_OPENSSL)
    if (mOpenSSLContext) {
        // should never happen, the OpenSSLContext class should not exist
        mOpenSSLContext = nullptr;
        ORV_ERROR(mContext, "Tried to set OpenSSLContext context, but library is built without OpenSSL support. Rejecting context.");
    }
#endif // OPENRV_HAVE_OPENSSL
}

void Socket::clearEncryptionContext()
{
    mOpenSSLContext = nullptr;
    mMbedTlsContext = nullptr;
}

static int getLastErrorCode()
{
#if !defined(_MSC_VER)
    return errno;
#else // _MSC_VER
    return WSAGetLastError();
#endif // _MSC_VER
}

/**
 * Get current time (relative to an @em undefined clock, i.e. can not be used as some kind of
 * calender time) in us. This value is meant to be @em monotonic.
 **/
static uint64_t getTimestampUs()
{
#if defined(_MSC_VER)
    // win32 does not have clock_gettime()
    // NOTE: QueryPerformanceCounter() and QueryPerformanceFrequency() may return non-zero
    //       on windows before XP only.
    //       on systems >= XP we can assume they always succeed.
    //       we do not support systems prior XP anyway.
    if (!g_time_win32_time_initialized) {
        std::atomic_thread_fence(std::memory_order_acquire); // the READ operation (on g_time_win32_time_initialized) must not be performed after this point
        static std::mutex m;
        std::lock_guard<std::mutex> lock(m);
        if (!g_time_win32_time_initialized) {
            QueryPerformanceFrequency(&g_time_win32_performance_frequency);
            g_time_win32_time_initialized = true;
        }
    }
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return (uint64_t)((t.QuadPart * 1000 * 1000) / g_time_win32_performance_frequency.QuadPart);
#elif defined(__MACH__)
    // MACH (OSX, iOS) does not implement clock_gettime()
    if (!g_time_mach_time_initialized) {
        // initialize global variables on the first call
        // NOTE: See also https://developer.apple.com/library/mac/qa/qa1398/_index.html
        std::atomic_thread_fence(std::memory_order_acquire); // the READ operation (on g_time_mach_time_initialized) must not be performed after this point
        static std::mutex m;
        std::lock_guard<std::mutex> lock(m);
        if (!g_time_mach_time_initialized) {
            mach_timebase_info_data_t t;
            mach_timebase_info(&t);
            g_time_mach_timebase = t.numer;
            g_time_mach_timebase /= t.denom;
            g_time_mach_timestart = mach_absolute_time();
            std::atomic_thread_fence(std::memory_order_release); // the READ/WRITE operations must not be performed after the following WRITE operation(s) (on g_time_mach_time_initialized).
            g_time_mach_time_initialized = true;
        }
    }
    uint64_t diffNano = (mach_absolute_time() - g_time_mach_timestart) * g_time_mach_timebase;
    return diffNano / 1000;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 * 1000 + ts.tv_nsec / 1000;
#endif
}

inline static void makeTimeout(uint64_t* timeoutSec, uint64_t* timeoutUsec, uint64_t lastActivityTimeUs, uint64_t currentTimeUs, uint64_t timeoutUs)
{
    if (currentTimeUs < lastActivityTimeUs) {
        currentTimeUs = lastActivityTimeUs;
    }
    uint64_t timePassedUs = currentTimeUs - lastActivityTimeUs;
    uint64_t remainingTimeUs = 0;
    if (timePassedUs < timeoutUs) {
        remainingTimeUs = timeoutUs - timePassedUs;

        // timeout will be used for select(), so make sure that select will return after the
        // timeout.
        remainingTimeUs += 50;
    }
    *timeoutSec = remainingTimeUs / (1000 * 1000);
    *timeoutUsec = remainingTimeUs % (1000 * 1000);
}

/**
 * Low-level receive function. This function is a wrapper around ::recv() or (if an encryption
 * context is set) SSL_read(), ...
 *
 * This function merely calls the receive function and transforms the return (error-) value (which
 * is platform specific) to an internal error value used by this class.
 * This function does @em NOT handle any error and does @em NOT count the received bytes, the caller
 * has to do so.
 *
 * @return The error state of the receive call.
 *         This is transformed from @ref getLastErrorCode() for "normal" connections and from the
 *         SSL error codes for encrypted connections.
 *         The platform-specific error (e.g. @ref getLastErrorCode() for unencrypted connections) is
 *         placed into @p lastError on errors. Note that on error @p bytesRead may be negative,
 *         also note that on success @p bytesRead is > 0 (a value of 0 causes @ref
 *         SendRecvSocketError::ClosedByRemote).
 **/
SendRecvSocketError Socket::receiveData(void* buf, size_t nbyte, ssize_t* bytesRead, int* lastError)
{
#if defined(OPENRV_HAVE_MBEDTLS)
    if (mMbedTlsContext) {
        // TODO
        //return mMbedTlsContext->receiveData(mContext, buf, nbyte, bytesRead, lastError);
    }
#endif
#if defined(OPENRV_HAVE_OPENSSL)
    if (mOpenSSLContext) {
        return mOpenSSLContext->receiveData(mContext, buf, nbyte, bytesRead, lastError);
    }
#endif
    const int flags = 0;
    ssize_t s = ::recv(mSocketFd, (char*)buf, nbyte, flags);
    *bytesRead = s;
    if (s == 0) {
        return SendRecvSocketError::ClosedByRemote;
    }
    if (s > 0) {
        return SendRecvSocketError::NoError;
    }
    *lastError = getLastErrorCode();
#if !defined(_MSC_VER)
    switch (*lastError) {
        case EAGAIN:
            // select() for more data
            return SendRecvSocketError::CallAgainWaitForRead;
        case ECONNRESET:
            return SendRecvSocketError::ResetByRemote;
        default:
            return SendRecvSocketError::GenericError;
    }
#else // _MSC_VER
    switch (*lastError) {
        case WSAEWOULDBLOCK:
            return SendRecvSocketError::CallAgainWaitForRead;
        case WSAECONNRESET:
            return SendRecvSocketError::ResetByRemote;
        default:
            return SendRecvSocketError::GenericError;
    }
#endif // _MSC_VER
    return SendRecvSocketError::InternalErrorUnreachableCode;
}

/**
 * Low-level send function. This function is a wrapper around ::send() or (if an encryption
 * context is set) SSL_write(), ...
 *
 * This function merely calls the send function and transforms the return (error-) value (which
 * is platform specific) to an internal error value used by this class.
 * This function does @em NOT handle any error and does @em NOT count the sent bytes, the caller
 * has to do so.
 *
 * @return The error state of the send call.
 *         This is transformed from @ref getLastErrorCode() for "normal" connections and from the
 *         SSL error codes for encrypted connections.
 *         The platform-specific error (e.g. @ref getLastErrorCode() for unencrypted connections) is
 *         placed into @p lastError on errors. Note that on error @p bytesSent may be negative.
 **/
SendRecvSocketError Socket::sendData(void* buf, size_t nbyte, ssize_t* bytesSent, int* lastError)
{
#if defined(OPENRV_HAVE_MBEDTLS)
    if (mMbedTlsContext) {
        // TODO
        //return mMbedTlsContext->sendData(mContext, buf, nbyte, bytesSent, lastError);
    }
#endif
#if defined(OPENRV_HAVE_OPENSSL)
    if (mOpenSSLContext) {
        return mOpenSSLContext->sendData(mContext, buf, nbyte, bytesSent, lastError);
    }
#endif
    int flags = 0;
#ifdef MSG_NOSIGNAL
    // do not send SIGPIPE if remote has closed the socket
    // NOTE: flag is supported by linux only (implemented as a #define, so we can #ifdef for it,
    //       in case other systems use it as well), OSX uses setsockopt() with SO_NOSIGPIPE
    //       instead.
    //       TODO: windows?
    flags |= MSG_NOSIGNAL;
#endif
    ssize_t s = ::send(mSocketFd, (char*)buf, nbyte, flags);
    *bytesSent = s;
    if (s >= 0) {
        return SendRecvSocketError::NoError;
    }
    *lastError = getLastErrorCode();
#if !defined(_MSC_VER)
    switch (*lastError) {
        case EAGAIN:
            return SendRecvSocketError::CallAgainWaitForWrite;
        case ECONNRESET:
            return SendRecvSocketError::ResetByRemote;
        default:
            return SendRecvSocketError::GenericError;
    }
#else // _MSC_VER
    switch (*lastError) {
        case WSAEWOULDBLOCK:
            return SendRecvSocketError::CallAgainWaitForWrite;
        case WSAECONNABORTED:
        case WSAECONNRESET:
            return SendRecvSocketError::ResetByRemote;
        default:
            return SendRecvSocketError::GenericError;
    }
#endif // _MSC_VER
    return SendRecvSocketError::InternalErrorUnreachableCode;
}

} // namespace openrv

