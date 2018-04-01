#include "rfb3xhandshake.h"
#include "socket.h"
#include "reader.h"
//#include "writer.h"
#include "orvclientdefines.h"
#include "orvvncclient.h"
#include "securitytypehandler.h"

#include <string.h>
#include <vector>
#include <algorithm>
#include <stdint.h>

namespace openrv {
namespace vnc {

/**
 * @param selectedProtocolVersion The RFB protocol version that should be used. Must be a valid enum
 *        entry and must be a 3.x version, otherwise the functions of this class have undefined
 *        behavior.
 * @param selectedProtocolVersionString The version string for @p selectedProtocolVersion.
 * @param socket Reference to the socket that should be used. Must remain valid for the lifetime of
 *        this object.
 * @param password Pointer to the password string. The data is @em NOT copied by this object and the
 *        pointer @em MUST remain valid for the lifetime of this object.
 * @param passwordLength Length in bytes of the password in @p password.
 * @param mbedTlsContext The mbedtls context, if encryption is supported, otherwise NULL.
 *        The context does not have to be initialized yet.
 *        The pointer must remain valid for the lifetime of this object. Ownership is NOT
 *        transferred.
 **/
Rfb3xHandshake::Rfb3xHandshake(orv_context_t* context, ProtocolVersion selectedProtocolVersion, const char* selectedProtocolVersionString, Socket& socket, const char* password, size_t passwordLength, MbedTlsContext* mbedTlsContext, OpenSSLContext* openSSLContext)
    : mContext(context),
      mSelectedProtocolVersion(selectedProtocolVersion),
      mSocket(socket),
      mPassword(password),
      mPasswordLength(passwordLength),
      mMbedTlsContext(mbedTlsContext),
      mOpenSSLContext(openSSLContext)
{
    memcpy(mSelectedProtocolVersionString, selectedProtocolVersionString, ORV_VNC_PROTOCOL_VERSION_STRING_LENGTH);
    mSelectedProtocolVersionString[ORV_VNC_PROTOCOL_VERSION_STRING_LENGTH] = '\0';
}

Rfb3xHandshake::~Rfb3xHandshake()
{
}

/**
 * @pre The protocol version has been received from the server, but not yet sent by the client.
 * @pre The selected protocol version is known and is a 3.x version.
 * @post The @ref selectedSecurityType() is initialized (to @ref SecurityType::Invalid on failure).
 * @post The @ref orv_vnc_server_capabilities_t::mSupportedSecurityTypesCount and the associated array
 *       are initialized (to an empty list if no types could be read).
 *
 * Perform the RFB 3.x connection handshake, i.e.
 * - finalize the protocol version negotiation by sending the client version string
 * - perform security type negotiation
 *
 * On success, the @p error is reset. On failure, @p error holds an error code.
 **/
void Rfb3xHandshake::startVncProtocol(orv_vnc_server_capabilities_t* serverCapabilities, orv_error_t* error)
{
    orv_error_reset(error);
    serverCapabilities->mSupportedSecurityTypesCount = 0;
    mSecurityTypesCount = 0;
    mSelectedVNCSecurityType = SecurityType::Invalid;
    // Finalize protocol version negotiation
    if (!mSocket.writeDataBlocking(mSelectedProtocolVersionString, 12, error)) {
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return;
        }
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 6, "VNC/RFB version negotiation failed: failed to write response to socket");
        return;
    }
    negotiateSecurityTypeAndAuthenticate(serverCapabilities, error);
}

/**
 * @post On success, the @ref orv_vnc_server_capabilities_t::mSupportedSecurityTypes has been set
 *       (but @em NOT copied to corresponding @ref orv_vnc_server_capabilities_t in
 *       @ref CommunicationData)
 * @post On success, the @ref ConnectionInfo::mSelectedVNCSecurityType has been set
 *       (but @em NOT copied to corresponding @ref ConnectionInfo in @ref CommunicationData)
 * @post On success, the @ref orv_vnc_server_capabilities_t::mSupportedEncodingCapabilities has been set
 *       (but @em NOT copied to corresponding @ref orv_vnc_server_capabilities_t in
 *       @ref CommunicationData),
 *       though possibly with the "partial" flag, indicating that further changes may be made.
 *
 * Implement the security type negotiation phase of the RFB 3.x protocol.
 **/
void Rfb3xHandshake::negotiateSecurityTypeAndAuthenticate(orv_vnc_server_capabilities_t* serverCapabilities, orv_error_t* error)
{
    ORV_DEBUG(mContext, "Negotiating security type");
    switch (mSelectedProtocolVersion) {
        case ProtocolVersion::RFB_3_3:
            negotiateSecurityTypeAndAuthenticateRfb33(serverCapabilities, error);
            return;
        case ProtocolVersion::RFB_3_7:
        case ProtocolVersion::RFB_3_8:
            negotiateSecurityTypeAndAuthenticateRfb37_38(serverCapabilities, error);
            return;
        default:
            break;
    }
    // caller should have checked this
    orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 10, "Internal error: Unimplemented RFB version");
    return;
}

/**
 * @pre The @ref mSelectedProtocolVersion is @ref ProtocolVersion::RFB_3_7 or @ref
 *      ProtocolVersion::RFB_3_8.
 * @post The @ref orv_vnc_server_capabilities_t::mSupportedSecurityTypesCount and the corresponding
 *       @ref orv_vnc_server_capabilities_t::mSupportedSecurityTypes is initialized (to a length of 0 if
 *       types could not be read).
 * @post The @ref mSelectedVNCSecurityType is set to a known and supported type on success (which is
 *       never @ref SecurityType::Invalid), or remains undefined (normally @ref
 *       SecurityType::Invalid) on error.
 **/
void Rfb3xHandshake::negotiateSecurityTypeAndAuthenticateRfb37_38(orv_vnc_server_capabilities_t* serverCapabilities, orv_error_t* error)
{
    mSecurityTypesCount = 0;
    mSelectedVNCSecurityType = SecurityType::Invalid;
    serverCapabilities->mSupportedSecurityTypesCount = 0;
    if (!mSocket.readDataBlocking(&mSecurityTypesCount, 1, error)) {
        switch (error->mErrorCode) {
            case ORV_ERR_USER_INTERRUPTION:
                break;
            default:
                orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 11, "Failed to read security types count");
                break;
        }
        return;
    }
    if (mSecurityTypesCount == 0) {
        // if security types count is 0, server is supposed to send 4 bytes reasonLength and
        // reasonLength bytes ascii string that explains why (e.g. we requested unsupported
        // version).
        std::vector<uint8_t> reasonBuffer;
        orv_error_t errorTmp;
        if (!Rfb3xHandshake::readReasonMessage(&mSocket, &reasonBuffer, &errorTmp)) {
            if (errorTmp.mErrorCode == ORV_ERR_USER_INTERRUPTION) {
                orv_error_copy(error, &errorTmp);
                return;
            }
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 12, "Server did not provide security types and did not send a proper reason message. Additional details: %s", errorTmp.mErrorMessage);
            return;
        }
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_SERVER_REJECTS_CONNECTION, 15, "Server rejected connection (no security types provided). Server provided reason: %s", reasonBuffer.data());
        return;
    }
    if (!mSocket.readDataBlocking(mSecurityTypesSupportedByServer, (size_t)mSecurityTypesCount, error)) {
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return;
        }
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 16, "Failed to read security types array from server, expected %d bytes", (int)mSecurityTypesCount);
        return;
    }
    ORV_DEBUG(mContext, "Server supports %d security types", (int)mSecurityTypesCount);
    serverCapabilities->mSupportedSecurityTypesCount = mSecurityTypesCount;
    bool securityTypeSupported[255] = {};
    for (int i = 0; i < (int)mSecurityTypesCount; i++) {
        uint8_t securityType = mSecurityTypesSupportedByServer[i];
        serverCapabilities->mSupportedSecurityTypes[i] = securityType;
        securityTypeSupported[securityType] = true;
        ORV_DEBUG(mContext, "  Server supports security type %d (%s)", (int)securityType, OrvVncClient::getSecurityTypeString((SecurityType)securityType));
    }

    // List of SecurityType values supported by this implementation.
    // In order of preference.
    const SecurityType supportedTypes[] = {
#if defined(OPENRV_HAVE_MBEDTLS)
        SecurityType::VeNCrypt,
#endif // OPENRV_HAVE_MBEDTLS
#if defined(OPENRV_HAVE_OPENSSL)
        SecurityType::AnonymousTLS,
#endif // OPENRV_HAVE_OPENSSL
        SecurityType::None,
        SecurityType::VNCAuthentication,
        SecurityType::Tight, // NOTE: this is preferred, but some servers provide tunnel types (that are not documented anywhere apparently) but NOT the "NOTUNNEL" type, thus we cannot connect to that server anymore. therefore we prefer the other securityTypes, which do not have tunnels.
    };
    mSelectedVNCSecurityType = SecurityType::Invalid;
    const int supportedTypesCount = sizeof(supportedTypes) / sizeof(SecurityType);
    for (int i = 0; i < supportedTypesCount; i++) {
        if (securityTypeSupported[(int)supportedTypes[i]]) {
            mSelectedVNCSecurityType = supportedTypes[i];
            break;
        }
    }
    if (mSelectedVNCSecurityType == SecurityType::Invalid) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_AUTHENTICATION_FAILED, 17, "Server does not support any authentication method (SecurityType) that this client supports.");
        return;
    }

    uint8_t selectedSecurityType = (uint8_t)mSelectedVNCSecurityType;
    ORV_DEBUG(mContext, "Selecting security type %d (%s)", (int)selectedSecurityType, OrvVncClient::getSecurityTypeString(mSelectedVNCSecurityType));
    if (!mSocket.writeDataBlocking(&selectedSecurityType, 1, error)) {
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return;
        }
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 18, "Failed to write security type to server socket");
        return;
    }

    if (error->mHasError) {
        return;
    }
    handleSelectedSecurityType(error);
}

/**
 * @pre The @ref mSelectedProtocolVersion is @ref ProtocolVersion::RFB_3_3
 * @post The @ref orv_vnc_server_capabilities_t::mSupportedSecurityTypesCount and the corresponding
 *       @ref orv_vnc_server_capabilities_t::mSupportedSecurityTypes is initialized (to a length of 0 if
 *       types could not be read).
 **/
void Rfb3xHandshake::negotiateSecurityTypeAndAuthenticateRfb33(orv_vnc_server_capabilities_t* serverCapabilities, orv_error_t* error)
{
    mSecurityTypesCount = 0;
    mSelectedVNCSecurityType = SecurityType::Invalid;
    serverCapabilities->mSupportedSecurityTypesCount = 0;
    char securityTypeBuffer[4];
    if (!mSocket.readDataBlocking(securityTypeBuffer, 4, error)) {
        switch (error->mErrorCode) {
            case ORV_ERR_USER_INTERRUPTION:
                break;
            default:
                orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 0, "Failed to read security type from server (RFB 3.3)");
                break;
        }
        return;
    }
    uint32_t securityType = Reader::readUInt32(securityTypeBuffer);
    if (securityType > 2) {
        // only 0, 1 and 2 are valid in RFB 3.3.
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 0, "Server selected invalid security type %u (RFB version 3.3)", (unsigned int)securityType);
        return;
    }
    mSecurityTypesCount = 1;
    serverCapabilities->mSupportedSecurityTypesCount = mSecurityTypesCount;
    serverCapabilities->mSupportedSecurityTypes[0] = (uint8_t)securityType;
    mSelectedVNCSecurityType = (SecurityType)securityType;

    if (error->mHasError) {
        return;
    }
    handleSelectedSecurityType(error);
}

/**
 * @pre @ref mSelectedVNCSecurityType has been initialized to a known value. If the type is @ref
 *      SecurityType::Invalid, the protocol version @em must be @ref ProtocolVersion::RFB_3_3.
 * @pre The @ref mSelectedProtocolVersion is a valid 3.x version.
 **/
void Rfb3xHandshake::handleSelectedSecurityType(orv_error_t* error)
{
    switch (mSelectedVNCSecurityType) {
        case SecurityType::Invalid:
        {
            // NOTE: Only valid in RFB 3.3.
            //       For RFB >= 3.7, we fail prior to this (Invalid is never selected by the client
            //       as a security type).
            if (mSelectedProtocolVersion != ProtocolVersion::RFB_3_3) {
                orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Internal error: Selected Invalid security type. This is allowed in RFB 3.3 only.");
                return;
            }
            // Server writes reason for failure
            std::vector<uint8_t> reasonMessage;
            orv_error_t errorTmp;
            if (!Rfb3xHandshake::readReasonMessage(&mSocket, &reasonMessage, &errorTmp)) {
                if (errorTmp.mErrorCode == ORV_ERR_USER_INTERRUPTION) {
                    orv_error_copy(error, &errorTmp);
                    return;
                }
                orv_error_set(error, ORV_ERR_CONNECT_ERROR_SERVER_REJECTS_CONNECTION, 0, "Server rejected connection. No reason available: Failed reading reason message (details: %s)", errorTmp.mErrorMessage);
                return;
            }
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_SERVER_REJECTS_CONNECTION, 0, "Server rejected connection. Reason reported by server: %s", reasonMessage.data());
            return;
        }
        case SecurityType::None:
        {
            SecurityTypeHandlerNone handler(mContext, &mSocket);
            handler.handle(error);

            // RFB 3.3 and 3.7 omit the SecurityResult message for SecurityType::None.
            switch (mSelectedProtocolVersion) {
                case ProtocolVersion::RFB_3_3:
                case ProtocolVersion::RFB_3_7:
                    return;
                default:
                    break;
            }

            break;
        }
        case SecurityType::VNCAuthentication:
        {
            SecurityTypeHandlerVNCAuthentication handler(mContext, &mSocket, mPassword, mPasswordLength);
            handler.handle(error);
            break;
        }
        case SecurityType::Tight:
        {
            SecurityTypeHandlerTight handler(mContext, &mSocket, mPassword, mPasswordLength);
            handler.handle(error);
            if (!error->mHasError) {
                // TODO: on success: store in server capabilities / connection info:
                // - tunnel capabilities <-- in orv_vnc_server_capabilities_t
                // - selected tunnel capability <-- in ConnectionInfo
                // - auth capabilities <-- in orv_vnc_server_capabilities_t
                // - selected auth capability <-- in ConnectionInfo
            }

            if (handler.selectedAuthTypeNone()) {
                // RFB 3.3 and 3.7 omit the SecurityResult message for SecurityType::None.
                switch (mSelectedProtocolVersion) {
                    case ProtocolVersion::RFB_3_3:
                        // cannot be reached: version 3.3 allows SecurityType None and VNCAuthentication
                        // only.
                    case ProtocolVersion::RFB_3_7:
                        return;
                    default:
                        break;
                }
            }

            break;
        }
        case SecurityType::AnonymousTLS:
        {
#if defined(OPENRV_HAVE_OPENSSL)
            SecurityTypeHandlerAnonymousTLS handler(mContext, &mSocket, mPassword, mPasswordLength, mOpenSSLContext);
            handler.handle(error);
            if (handler.selectedSecuritySubType() == SecurityType::None) {
                // RFB 3.3 and 3.7 omit the SecurityResult message for SecurityType::None.
                switch (mSelectedProtocolVersion) {
                    case ProtocolVersion::RFB_3_3:
                        // cannot be reached: version 3.3 allows SecurityType None and VNCAuthentication
                        // only.
                    case ProtocolVersion::RFB_3_7:
                        return;
                    default:
                        break;
                }
            }
            break;
#else // OPENRV_HAVE_OPENSSL
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 0, "Internal error: Selected security %d (%s) type not supported (anonymous encryption not enabled in library)", (int)mSelectedVNCSecurityType, OrvVncClient::getSecurityTypeString(mSelectedVNCSecurityType));
            return;
#endif // OPENRV_HAVE_MBEDTLS
        }
        case SecurityType::VeNCrypt:
        {
#if defined(OPENRV_HAVE_MBEDTLS)
            SecurityTypeHandlerVeNCrypt handler(mContext, &mSocket, mPassword, mPasswordLength, mMbedTlsContext);
            handler.handle(error);
#else // OPENRV_HAVE_MBEDTLS
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 0, "Internal error: Selected security %d (%s) type not supported (encryption not enabled in library)", (int)mSelectedVNCSecurityType, OrvVncClient::getSecurityTypeString(mSelectedVNCSecurityType));
            return;
#endif // OPENRV_HAVE_MBEDTLS
            break;
        }
        default:
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 0, "Internal error: Selected security %d (%s) type not implemented", (int)mSelectedVNCSecurityType, OrvVncClient::getSecurityTypeString(mSelectedVNCSecurityType));
            return;
    }

    if (error->mHasError) {
        return;
    }

    //
    // SecurityResult message
    //
    char securityResultBuffer[4];
    if (!mSocket.readDataBlocking(securityResultBuffer, 4, error)) {
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return;
        }
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 1000, "Failed to read SecurityResult message.");
        return;
    }
    uint32_t securityResult = Reader::readUInt32(securityResultBuffer);
    // only 0 and 1 are valid responses
    // (2 as well, if Tight security type is enabled)
    if (mSelectedVNCSecurityType == SecurityType::Tight && securityResult == 2) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_SERVER_REJECTS_CONNECTION, 1100, "Connection rejected, too many attempts.");
        return;
    }
    if (securityResult != 0 && securityResult != 1) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 1001, "SecurityResult message sent by server is invalid");
        return;
    }
    if (securityResult != 0) {
        ORV_DEBUG(mContext, "SecurityResult message indicates failure");
        if ((int)mSelectedProtocolVersion < (int)ProtocolVersion::RFB_3_8) {
            // in RFB < 3.8 the server closes the connection on failure, no reason is sent.
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_AUTHENTICATION_FAILED, 0, "Authentication failed");
            return;
        }
        // Server writes reason for failure
        std::vector<uint8_t> reasonMessage;
        orv_error_t errorTmp;
        if (!Rfb3xHandshake::readReasonMessage(&mSocket, &reasonMessage, &errorTmp)) {
            if (errorTmp.mErrorCode == ORV_ERR_USER_INTERRUPTION) {
                orv_error_copy(error, &errorTmp);
                return;
            }
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_AUTHENTICATION_FAILED, 0, "Authentication failed. No reason available: Failed to read reason message from server (details: %s)", errorTmp.mErrorMessage);
            return;
        }
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_AUTHENTICATION_FAILED, 0, "Authentication failed. Reason reported by server: %s", reasonMessage.data());
        return;
    }
    // success
    ORV_DEBUG(mContext, "SecurityResult message indicates success");
}


/**
 * Read a "reason" message from @p socket and place it to @p reasonBuffer.
 *
 * A "reason" message is a 4 byte length followed by a length bytes string message. This is normally
 * sent when the server rejects a connect for some reason.
 *
 * @return TRUE on success, otherwise FALSE. On success, @p reasonBuffer holds a NUL-terminated
 *         reason message, otherwise @p error holds an error code.
 **/
bool Rfb3xHandshake::readReasonMessage(Socket* socket, std::vector<uint8_t>* reasonBuffer, orv_error_t* error)
{
    char reasonLengthBuffer[4];
    if (!socket->readDataBlocking(reasonLengthBuffer, 4, error)) {
        reasonBuffer->clear();
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return false;
        }
        orv_error_set(error, ORV_ERR_PROTOCOL_ERROR, 0, "Expected reason message, but could not read length of message.");
        return false;
    }
    uint32_t reasonLength = Reader::readUInt32(reasonLengthBuffer);
    if (reasonLength > ORV_MAX_REASON_LENGTH) {
        reasonBuffer->clear();
        orv_error_set(error, ORV_ERR_PROTOCOL_ERROR, 0, "Server sent a reason message of length %u, which exceeds the hardcoded length limit.", (unsigned int)reasonLength);
        return false;
    }
    reasonBuffer->resize(reasonLength + 1);
    if (!socket->readDataBlocking(reasonBuffer->data(), (size_t)reasonLength, error)) {
        reasonBuffer->clear();
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return false;
        }
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 0, "Failed to read %u bytes of reason message from remote", (unsigned int)reasonLength);
        return false;
    }
    orv_error_reset(error);
    (*reasonBuffer)[reasonLength] = 0;
    return true;
}

} // namespace vnc
} // namespace openrv

