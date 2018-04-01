#ifndef OPENRV_OPENSSLCONTEXT_H
#define OPENRV_OPENSSLCONTEXT_H

#include <mutex>
#include <openssl/ssl.h>
#include <libopenrv/orv_error.h>

namespace openrv {

enum class SendRecvSocketError;

class OpenSSLContext
{
public:
    OpenSSLContext();
    virtual ~OpenSSLContext();

    bool init(orv_context_t* orvContext, orv_error_t* error);
    bool isInitialized() const;
    bool outputOpensSSLErrorsAndClearQueue(orv_context_t* orvContext, const char* hint);
    bool makeAnonymousTLS();

    SSL* ssl() const;

    void shutdown();
    SendRecvSocketError receiveData(orv_context_t* orvContext, void* buf, size_t nbyte, ssize_t* bytesRead, int* lastError);
    SendRecvSocketError sendData(orv_context_t* orvContext, void* buf, size_t nbyte, ssize_t* bytesRead, int* lastError);

protected:
    void initOpenSSLGlobal();

private:
    bool mInitialized = false;
    SSL_CTX* mContext = nullptr;
    const SSL_METHOD* mMethod = nullptr;
    SSL* mSSL = nullptr;
};

inline bool OpenSSLContext::isInitialized() const
{
    return mInitialized;
}

inline SSL* OpenSSLContext::ssl() const
{
    return mSSL;
}

} // namespace openrv

#endif

