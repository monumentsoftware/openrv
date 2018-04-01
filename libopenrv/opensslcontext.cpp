#include "opensslcontext.h"

#include <libopenrv/libopenrv.h>
#include "socket.h"
#include <openssl/conf.h>
#include <openssl/err.h>
#include <stdio.h>


namespace openrv {

struct OpenSSLGlobal
{
    std::mutex mInitMutex;
    bool mInitialized = false;
    int mOpenSSLLocksCount = 0;
    std::mutex* mOpenSSLLocks = nullptr;
};

static OpenSSLGlobal g_OpenSSLGlobalData;

static void openssl_locking_function(int mode, int n, const char* file, int line)
{
    (void)file;
    (void)line;
    if (mode & CRYPTO_LOCK) {
        g_OpenSSLGlobalData.mOpenSSLLocks[n].lock();
    }
    else {
        g_OpenSSLGlobalData.mOpenSSLLocks[n].unlock();
    }
}

OpenSSLContext::OpenSSLContext()
{
}

OpenSSLContext::~OpenSSLContext()
{
    if (mInitialized) {
        if (mSSL) {
            SSL_free(mSSL);
            mSSL = nullptr;
        }
        if (mContext) {
            SSL_CTX_free(mContext);
            mContext = nullptr;
        }

        // TODO: free global resource if they are no longer needed
        //       -> difficult to figure out when to do this exactly:
        //          we could do reference counting on our contexts (and simply re-create data when
        //          needed again), but we can't be sure that the user may depend on the data as
        //          well!
        std::unique_lock<std::mutex> lock(g_OpenSSLGlobalData.mInitMutex);
        // NOTE: ERR_free_strings(); might make sense here, however we cannot guarantee that the
        //       user-code doesn't use OpenSSL as well, so we have to avoid it.
        //       also note. in OpenSSL >= 1.1.0, ERR_free_strings() does not do anything anymore.
    }
}

bool OpenSSLContext::init(orv_context_t* orvContext, orv_error_t* error)
{
    orv_error_reset(error);
    if (isInitialized()) {
        return error->mHasError;
    }
    initOpenSSLGlobal();

    outputOpensSSLErrorsAndClearQueue(orvContext, "before OpenSSL context creation");

    mMethod = SSLv23_method();
    if (mMethod == nullptr) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error: Failed to retrieve OpenSSL method");
        return error->mHasError;
    }

    mContext = SSL_CTX_new(mMethod);
    if (mContext == nullptr) {
        mMethod = nullptr;
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error: Failed to create OpenSSL context");
        return false;
    }

    // Disable SSLv2/SSLv3 and SSL compression for security reasons (known security leaks)
    const long flags = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION;
    SSL_CTX_set_options(mContext, flags);

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    // Require TLS 1.0 (no SSLv2 or SSLv3)
    // TODO: allow to increase to minimum TLS 1.2
    if (SSL_CTX_set_min_proto_version(mContext, TLS1_VERSION) != 1) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Internal error: Failed to set minimum SSL version on OpenSSL context");
        SSL_CTX_free(mContext);
        mContext = nullptr;
        mMethod = nullptr;
        return false;
    }
#endif // OPENSSL_VERSION_NUMBER >= 0x10100000L

    outputOpensSSLErrorsAndClearQueue(orvContext, "Before SSL_new()");

    mSSL = SSL_new(mContext);
    if (!mSSL) {
        outputOpensSSLErrorsAndClearQueue(orvContext, "After SSL_new()");
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Failed to create SSL object for context");
        SSL_CTX_free(mContext);
        mContext = nullptr;
        mMethod = nullptr;
        return false;
    }

    mInitialized = true;
    return true;
}

void OpenSSLContext::initOpenSSLGlobal()
{
    std::unique_lock<std::mutex> lock(g_OpenSSLGlobalData.mInitMutex);
    if (g_OpenSSLGlobalData.mInitialized) {
        return;
    }

    // FIXME: thread-safety: how can we make sure the user-code is not currently calling this as well?
    SSL_library_init(); // NOTE: deprecated in OpenSSL 1.1.0 (no explicit initialization required anymore)
    SSL_load_error_strings(); // NOTE: deprecated in OpenSSL 1.1.0 (no explicit initialization required anymore)
    OPENSSL_config(nullptr);
    OpenSSL_add_all_algorithms();

    g_OpenSSLGlobalData.mOpenSSLLocksCount = CRYPTO_num_locks();
    g_OpenSSLGlobalData.mOpenSSLLocks = new std::mutex[g_OpenSSLGlobalData.mOpenSSLLocksCount];
    g_OpenSSLGlobalData.mInitialized = true;
    CRYPTO_set_locking_callback(openssl_locking_function);
}

 /**
  * Send an SSL close to remote and return immediately. Any errors are ignored, this function
  * assumes that the socket will be closed immediately after this call, so this function does @em
  * not guarantee that the close was actually sent.
  **/
void OpenSSLContext::shutdown()
{
    if (mSSL) {
        SSL_shutdown(mSSL);
    }
}

/**
 * Check the OpenSSL error queue of the current thread and output all errors using ORV_ERROR.
 *
 * The error queue is cleared by this function.
 *
 * @return TRUE if any OpenSSL errors were encountered, otherwise FALSE.
 **/
bool OpenSSLContext::outputOpensSSLErrorsAndClearQueue(orv_context_t* orvContext, const char* hint)
{
    bool hadError = false;
    unsigned long sslError = 0;
    while ((sslError = ERR_get_error()) != 0) {
        const size_t bufferSize = 1024;
        char buffer[bufferSize + 1];
        buffer[bufferSize] = 0;
        ERR_error_string_n(sslError, buffer, bufferSize);
        ORV_ERROR(orvContext, "OpenSSL has error code in queue (%s): %lu, %s", hint, sslError, buffer);
        hadError = true;
    }
    return hadError;
}

/**
 * Make the @ref ssl() object accept anonymous TLS connections. This must be used prior to starting
 * to use the @ref ssl() object and after calling @ref init().
 *
 * NOTE: "Anonymous" refers to the fact that the client does @em NOT perform ANY certificate checks
 *       on the remote server. Consequently man-in-the-middle attacks are inherantly easy on
 *       anonymous TLS connections.
 *       Consequently calling this function should be avoided whenever possibl, the resulting
 *       connection is NOT secure. OpenSSL will merely encrypt the data, but any attacker that could
 *       act as a proxy can read and modify all data. (In most cases acting as a proxy is among
 *       the easiest attacks possible - if you think this is not possible for an attacker, why are
 *       trying to encrypt the data anyway? If the remote cannot act as a proxy, he normally cannot
 *       read the data anyway)
 **/
bool OpenSSLContext::makeAnonymousTLS()
{
    if (!mSSL) {
        return false;
    }
    if (SSL_set_cipher_list(mSSL, "ADH") == 1) {
        return true;
    }
    return false;
}

/**
 * Low-level recv function used by @ref Socket::receiveData() for OpenSSL connections.
 *
 * See @ref Socket::receiveData() for details.
 **/
SendRecvSocketError OpenSSLContext::receiveData(orv_context_t* orvContext, void* buf, size_t nbyte, ssize_t* bytesRead, int* lastError)
{
    if (!mSSL) {
        *lastError = EINVAL;
        return SendRecvSocketError::GenericError;
    }
    // TODO: currently we only count the # of bytes sent/received on the VNC level, it would be nice
    //       if we would also count the bytes actually sent over the socket.
    //       -> for unencrypted connections this is the same, but for SSL connections the values
    //          normally differ (SSL negotiations/renegotiations at the very least)
    int ret = SSL_read(mSSL, buf, nbyte);
    *bytesRead = (ssize_t)ret;
    if (ret > 0) {
        return SendRecvSocketError::NoError;
    }
    *lastError = SSL_get_error(mSSL, ret);
    if (ret == 0 && *lastError != SSL_ERROR_ZERO_RETURN) {
        return SendRecvSocketError::ResetByRemote;
    }
    switch (*lastError) {
        case SSL_ERROR_ZERO_RETURN: // should occur only if ret==0 and indicates a "clean" shutdown
            return SendRecvSocketError::ClosedByRemote;
        case SSL_ERROR_WANT_READ:
            return SendRecvSocketError::CallAgainWaitForRead;
        case SSL_ERROR_WANT_WRITE:
            // WARNING: read *or* write may be required by SSL, i.e. in particular: a receiveData()
            //          call may result in SSL requiring to *write* data!
            return SendRecvSocketError::CallAgainWaitForWrite;
        case SSL_ERROR_SYSCALL:
            return SendRecvSocketError::GenericError;
        default:
        case SSL_ERROR_SSL:
            // additional details are in the error queue
            outputOpensSSLErrorsAndClearQueue(orvContext, "After SSL_read()");
            return SendRecvSocketError::GenericError;
    }
    return SendRecvSocketError::InternalErrorUnreachableCode;
}

/**
 * Low-level send function used by @ref Socket::sendData() for OpenSSL connections.
 *
 * See @ref Socket::sendData() for details.
 **/
SendRecvSocketError OpenSSLContext::sendData(orv_context_t* orvContext, void* buf, size_t nbyte, ssize_t* bytesRead, int* lastError)
{
    if (!mSSL) {
        *lastError = EINVAL;
        return SendRecvSocketError::GenericError;
    }
    // TODO: currently we only count the # of bytes sent/received on the VNC level, it would be nice
    //       if we would also count the bytes actually sent over the socket.
    //       -> for unencrypted connections this is the same, but for SSL connections the values
    //          normally differ (SSL negotiations/renegotiations at the very least)
    int ret = SSL_write(mSSL, buf, nbyte);
    *bytesRead = (ssize_t)ret;
    if (ret > 0) {
        return SendRecvSocketError::NoError;
    }
    *lastError = SSL_get_error(mSSL, ret);
    if (ret == 0 && *lastError != SSL_ERROR_ZERO_RETURN) {
        return SendRecvSocketError::ResetByRemote;
    }
    switch (*lastError) {
        case SSL_ERROR_ZERO_RETURN: // should occur only if ret==0 and indicates a "clean" shutdown
            return SendRecvSocketError::ClosedByRemote;
        case SSL_ERROR_WANT_READ:
            // WARNING: read *or* write may be required by SSL, i.e. in particular: a sendData()
            //          call may result in SSL requiring to *read* data!
            return SendRecvSocketError::CallAgainWaitForRead;
        case SSL_ERROR_WANT_WRITE:
            return SendRecvSocketError::CallAgainWaitForWrite;
        case SSL_ERROR_SYSCALL:
            return SendRecvSocketError::GenericError;
        default:
        case SSL_ERROR_SSL:
            // additional details are in the error queue
            outputOpensSSLErrorsAndClearQueue(orvContext, "After SSL_read()");
            return SendRecvSocketError::GenericError;
    }
    return SendRecvSocketError::InternalErrorUnreachableCode;
}

} // namespace openrv

