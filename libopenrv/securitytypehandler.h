#ifndef OPENRV_SECURITYTYPEHANDLER_H
#define OPENRV_SECURITYTYPEHANDLER_H

#include <libopenrv/orv_config.h>
#include "rfbtypes.h"

#include <stdint.h>
#include <stdlib.h>

struct orv_vnc_tight_capability_t;
struct orv_error_t;
struct orv_context_t;

namespace openrv {

class Socket;
class MbedTlsContext;
class OpenSSLContext;

namespace vnc {

/**
 * Base class of helper classes to handle the SecurityType-specific steps of the handshaking
 * protocol.
 **/
class SecurityTypeHandler
{
public:
    SecurityTypeHandler(struct orv_context_t* context, Socket* socket, const char* password, size_t passwordLength);
    virtual ~SecurityTypeHandler() = default;

    virtual void handle(struct orv_error_t* error) = 0;

protected:
    struct orv_context_t* mContext = nullptr;
    Socket* mSocket = nullptr;
    const char* mPassword = nullptr;
    const size_t mPasswordLength;
};

class SecurityTypeHandlerNone : public SecurityTypeHandler
{
public:
    SecurityTypeHandlerNone(struct orv_context_t* context, Socket* socket);
    virtual ~SecurityTypeHandlerNone() = default;

    virtual void handle(struct orv_error_t* error);
};

class SecurityTypeHandlerVNCAuthentication : public SecurityTypeHandler
{
public:
    SecurityTypeHandlerVNCAuthentication(struct orv_context_t* context, Socket* socket, const char* password, size_t passwordLength);
    virtual ~SecurityTypeHandlerVNCAuthentication() = default;

    virtual void handle(struct orv_error_t* error);
};

class SecurityTypeHandlerTight : public SecurityTypeHandler
{
public:
    SecurityTypeHandlerTight(struct orv_context_t* context, Socket* socket, const char* password, size_t passwordLength);
    ~SecurityTypeHandlerTight();

    virtual void handle(struct orv_error_t* error);
    bool selectedAuthTypeNone() const;

protected:
    void clear();
    void handleTunnels(struct orv_error_t* error);
    void handleAuthTypes(struct orv_error_t* error);

private:
    uint32_t mReportedNumberOfTunnels = 0;
    struct orv_vnc_tight_capability_t* mReportedTunnelCapabilities = nullptr;
    int32_t mSelectedTunnelCapabilityCode = 0;

    uint32_t mReportedNumberOfAuthTypes = 0;
    struct orv_vnc_tight_capability_t* mReportedAuthCapabilities = nullptr;
    int32_t mSelectedAuthCapabilityCode = 0;
};

#if defined(OPENRV_HAVE_MBEDTLS)
/**
 * Handler for the "VeNCrypt" security type.
 **/
class SecurityTypeHandlerVeNCrypt : public SecurityTypeHandler
{
public:
    SecurityTypeHandlerVeNCrypt(struct orv_context_t* context, Socket* socket, const char* password, size_t passwordLength, MbedTlsContext* mbedTlsContext);
    virtual ~SecurityTypeHandlerVeNCrypt();

    virtual void handle(struct orv_error_t* error);

private:
    void clear();

private:
    MbedTlsContext* mMbedTlsContext = nullptr;
};
#endif // OPENRV_HAVE_MBEDTLS

#if defined(OPENRV_HAVE_OPENSSL)
/**
 * Handler for the "TLS" security type, as used by the "vino" VNC server (part of GNOME). Note that
 * this security type was added by vino and is almost certainly vino-specific, other servers
 * implementing that type almost always also support at least VeNCrypt or other alternatives, which
 * should be preferred over this type, unless it is impossible to do so.
 *
 * The "TLS" security type in VNC performs anonymous TLS only, i.e. NOT secured by any certificates
 * and therefore does not protect in any way against man in the middle attacks. The connection is
 * encrypted only, but an attacker can easily intercept the connection and
 * simply act as a proxy between the client and the real server (= man in the middle), therefore
 * this type provides very little protection.
 *
 * Note that this is NOT the VeNCrypt security type (used by tigervnc and also supported by
 * libvncserver).
 *
 * The implementation for this type currently uses OpenSSL instead of mbedtls, because mbedtls does
 * not support anonymous TLS.
 **/
class SecurityTypeHandlerAnonymousTLS : public SecurityTypeHandler
{
public:
    SecurityTypeHandlerAnonymousTLS(struct orv_context_t* context, Socket* socket, const char* password, size_t passwordLength, OpenSSLContext* openSSLContext);
    virtual ~SecurityTypeHandlerAnonymousTLS();

    virtual void handle(struct orv_error_t* error);

    SecurityType selectedSecuritySubType() const;

protected:
    void startOpenSSLHandshake(orv_error_t* error);

private:
    void clear();

private:
    OpenSSLContext* mOpenSSLContext = nullptr;
    uint8_t mSecuritySubTypesCount = 0;
    uint8_t mSecuritySubTypesSupportedByServer[255] = {};
    SecurityType mSelectedSecuritySubType = SecurityType::Invalid;
};

/**
 * @return The security sub-type that was selected for authentication by this handler, if @ref
 *         handle() was finished successfully.
 *         Normally, this is always @ref SecurityType::None or @ref
 *         SecurityType::VNCAuthentication for anonymous TLS.
 **/
inline SecurityType SecurityTypeHandlerAnonymousTLS::selectedSecuritySubType() const
{
    return mSelectedSecuritySubType;
}
#endif // OPENRV_HAVE_OPENSSL

} // namespace vnc
} // namespace openrv

#endif

