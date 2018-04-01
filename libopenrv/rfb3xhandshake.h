#ifndef OPENRV_RFB3XHANDSHAKE_H
#define OPENRV_RFB3XHANDSHAKE_H

#include <libopenrv/libopenrv.h>
#include "rfbtypes.h"
#include <vector>

namespace openrv {

class Socket;
class MbedTlsContext;
class OpenSSLContext;

namespace vnc {

/**
 * Helper class for @ref openrv::vnc::ConnectionThread that implements the connection handshake of
 * RFB 3.x (3.3, 3.7 and 3.8).
 *
 *
 * This class assumes that the protocol version has already been received by the server, but no more
 * protocol steps have been performed.
 **/
class Rfb3xHandshake
{
public:
    Rfb3xHandshake(orv_context_t* context, ProtocolVersion selectedProtocolVersion, const char* selectedProtocolVersionString, Socket& socket, const char* password, size_t passwordLength, MbedTlsContext* mbedTlsContext, OpenSSLContext* openSSLContext);
    virtual ~Rfb3xHandshake();

    void startVncProtocol(orv_vnc_server_capabilities_t* serverCapabilities, orv_error_t* error);

    SecurityType selectedSecurityType() const;

    static bool readReasonMessage(Socket* socket, std::vector<uint8_t>* reasonBuffer, orv_error_t* error);

protected:
    void negotiateSecurityTypeAndAuthenticate(orv_vnc_server_capabilities_t* serverCapabilities, orv_error_t* error);
    void negotiateSecurityTypeAndAuthenticateRfb37_38(orv_vnc_server_capabilities_t* serverCapabilities, orv_error_t* error);
    void negotiateSecurityTypeAndAuthenticateRfb33(orv_vnc_server_capabilities_t* serverCapabilities, orv_error_t* error);
    void handleSelectedSecurityType(orv_error_t* error);

private:
    orv_context_t* mContext = nullptr;
    const ProtocolVersion mSelectedProtocolVersion;
    Socket& mSocket;
    char mSelectedProtocolVersionString[ORV_VNC_PROTOCOL_VERSION_STRING_LENGTH + 1] = {};
    const char* mPassword = nullptr; // NOTE: pointer to memory of owning object, memory NOT owned by this object
    const size_t mPasswordLength;

    uint8_t mSecurityTypesCount = 0;
    uint8_t mSecurityTypesSupportedByServer[255] = {};
    SecurityType mSelectedVNCSecurityType = SecurityType::Invalid;
    MbedTlsContext* mMbedTlsContext = nullptr;
    OpenSSLContext* mOpenSSLContext = nullptr; // NOTE: Normally mbedtls is used, not OpenSSL. This is used only when mbedtls does not support something we need.
};

/**
 * @return The selected security type, if @ref startVncProtocol() was successful, otherwise
 *         SecurityType::Invalid.
 **/
inline SecurityType Rfb3xHandshake::selectedSecurityType() const
{
    return mSelectedVNCSecurityType;
}

} // namespace vnc
} // namespace openrv

#endif

