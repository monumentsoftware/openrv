/*
 * Copyright (C) 2018 Monument-Software GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OPENRV_SOCKET_H
#define OPENRV_SOCKET_H

#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>
#include <mutex>

struct orv_error_t;
struct orv_context_t;

namespace openrv {

class ThreadNotifierListener;
namespace vnc {
    struct OrvVncClientSharedData;
}
class MbedTlsContext;
class OpenSSLContext;

/**
 * Enum used by @ref Socket, @ref OpenSSLContext and @ref MbedTlsContext as return value for send
 * and receive fucntions.
 **/
enum class SendRecvSocketError
{
    NoError,
    /**
     * The sendData()/receiveData() function should be called again when the socket is
     * readable again.
     *
     * On unencrypted connections, this is returned by receiveData() only. On encrypted connections,
     * this may be returned by sendData() as well (despite the function trying to @em write data),
     * because the SSL library may need to perform SSL renegotiation. The caller must then select on
     * the socket being @em readable before calling sendData() again.
     **/
    CallAgainWaitForRead,
    /**
     * The sendData()/receiveData() function should be called again when the socket is
     * writable again.
     *
     * On unencrypted connections, this is returned by sendData() only. On encrypted connections,
     * this may be returned by receiveData() as well (despite the function trying to @em read data),
     * because the SSL library may need to perform SSL renegotiation. The caller must then select on
     * the socket being @em writable before calling receiveData() again.
     **/
    CallAgainWaitForWrite,
    ClosedByRemote,        // orderly shutdown, recv() returns 0
    ResetByRemote,         // remote forced close, recv() returns error with errno=ECONNRESET
    GenericError,
    InternalErrorUnreachableCode,
};

/**
 * Wrapper class for a socket fd.
 *
 * Internally this class uses non-blocking sockets and provides a blocking socket API. All blocking
 * socket calls in this class return either if
 * - the operation has completed
 * - an error occurred
 * - an external thread (through a pipe) signals that the operation should be aborted
 *
 * This class is @em NOT thread safe. An object of this class may be used by a single thread at any
 * given time only.
 **/
class Socket
{
public:
    enum class WaitRet {
        /**
        * The socket has been signalled, i.e. data is available to read or can be written.
        **/
        Signalled,
        /**
        * User has requested to abort.
        **/
        UserInterruption,
        /**
        * Error has occurred and the lastError parameter has been set accordingly (errno or WSAGetLastError()).
        **/
        Error,
        /**
        * Timeout has occurred
        **/
        Timeout
    };
    enum class WaitType {
        Read,
        Write,
        Connect,
        /**
        * Wait on the @ref ThreadNotifierListener only, ignore the socket.
        **/
        NoSocketWait
    };
public:
    Socket(orv_context_t* ctx, ThreadNotifierListener* pipeListener, vnc::OrvVncClientSharedData* communicationData);
    Socket(orv_context_t* ctx, ThreadNotifierListener* pipeListener, std::mutex* sharedDataMutex, bool* sharedDataUserRequestedDisconnect);
    virtual ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    bool makeSocket(int addressFamily, struct orv_error_t* error);
    void close();
    bool writeDataBlocking(const void* buf, size_t nbyte, orv_error_t* error);
    bool readDataBlocking(void* buf, size_t nbyte, orv_error_t* error);
    uint32_t readAvailableDataNonBlocking(void* buf, size_t nbyte, SendRecvSocketError* callAgainType, orv_error_t* error);
    bool makeSocketAndConnectBlockingTo(const char* hostName, uint16_t port, orv_error_t* error);

    void setEncryptionContext(MbedTlsContext* mbedTlsContext);
    void setEncryptionContext(OpenSSLContext* openSSLContext);
    void clearEncryptionContext();

    bool isOpened() const;
    int socketFd() const;
    void setSocketTimeoutSeconds(int timeoutSeconds);
    int socketTimeoutSeconds() const;
    uint64_t socketReadTimeoutUs() const;
    uint64_t socketWriteTimeoutUs() const;
    uint64_t socketConnectTimeoutUs() const;

    void resetStatistics();
    size_t receivedBytes() const;
    size_t sentBytes() const;

    WaitRet waitForSignal(uint64_t timeoutSec, uint64_t timeoutUsec, bool useTimeout, WaitType waitType, int* lastError, bool* signalledSocket = nullptr, bool* signalledPipe = nullptr);

protected:
protected:
    static void makeConnectError(orv_error_t* error, const char* hostName, uint16_t port, int errorCode);
    static void makeConnectSelectError(orv_error_t* error, const char* hostName, uint16_t port, int errorCode);

    // low-level function, simply calls ::recv(), SSL_read(), ... and transforms return value. Does
    // NOT count received bytes or handle the error (caller must do so)
    SendRecvSocketError receiveData(void* buf, size_t nbyte, ssize_t* bytesRead, int* lastError);
    SendRecvSocketError sendData(void* buf, size_t nbyte, ssize_t* bytesSent, int* lastError);

private:
    orv_context_t* mContext = nullptr;
    ThreadNotifierListener* mPipeListener = nullptr;
    std::mutex* mCommunicationDataMutex = nullptr;
    bool* mCommunicationDataUserRequestedDisconnect = nullptr;
    int mSocketFd = -1;
    int mSocketTimeoutSeconds = 10;
#ifdef _MSC_VER
    void* mSocketEvent = nullptr;
#endif // _MSC_VER
    MbedTlsContext* mMbedTlsContext = nullptr;
    OpenSSLContext* mOpenSSLContext = nullptr;

    // NOTE: does NOT include TCP protocol overhead
    // NOTE: does NOT include SSL protocol overhead
    size_t mReceivedBytes = 0;
    size_t mSentBytes = 0;
};

/**
 * @return TRUE if this object has a socket fd, otherwise FALSE. Note that this function does not
 *         check if the fd is still valid.
 *         See also @ref makeSocket() and @ref close().
 **/
inline bool Socket::isOpened() const
{
    if (mSocketFd != -1) {
        return true;
    }
    return false;
}

/**
 * @return The internal fd of the socket. -1 if the socket was not yet opened or was closed.
 **/
inline int Socket::socketFd() const
{
    return mSocketFd;
}

/**
 * @return The number of bytes received on this socket by recv() calls, see @ref
 *         readDataBlocking()and @ref readAvailableDataNonBlocking().
 *         This does @em NOT call TCP protocol overhead.
 **/
inline size_t Socket::receivedBytes() const
{
    return mReceivedBytes;
}

/**
 * @return The number of bytes sent on this socket using send() calls, see @ref
 *         writeDataBlocking().
 *         This does @em NOT call TCP protocol overhead.
 **/
inline size_t Socket::sentBytes() const
{
    return mSentBytes;
}
inline int Socket::socketTimeoutSeconds() const
{
    return mSocketTimeoutSeconds;
}
inline uint64_t Socket::socketReadTimeoutUs() const
{
    return (uint64_t)socketTimeoutSeconds() * 1000 * 1000;
}
inline uint64_t Socket::socketWriteTimeoutUs() const
{
    return (uint64_t)socketTimeoutSeconds() * 1000 * 1000;
}
inline uint64_t Socket::socketConnectTimeoutUs() const
{
    return (uint64_t)socketTimeoutSeconds() * 1000 * 1000;
}

} // namespace openrv

#endif

