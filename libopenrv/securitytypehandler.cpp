#include "securitytypehandler.h"

#include <libopenrv/libopenrv.h>
#include "vncdes.h"
#include "orvvncclient.h"
#include "reader.h"
#include "writer.h"
#include "socket.h"
#include "orvclientdefines.h"
#include "rfb3xhandshake.h"
#if defined(OPENRV_HAVE_MBEDTLS)
#include "mbedtlscontext.h"
#include <mbedtls/ssl.h>
#include <mbedtls/net.h>
#include <mbedtls/error.h>
#include <mbedtls/debug.h>
#endif // OPENRV_HAVE_MBEDTLS
#if defined(OPENRV_HAVE_OPENSSL)
#include "opensslcontext.h"
//#include <openssl/x509.h>
#endif // OPENRV_HAVE_OPENSSL

#include <string.h>
#include <algorithm>
#include <vector>

#define UNUSED(x) (void)x

namespace openrv {
namespace vnc {

#if defined(OPENRV_HAVE_MBEDTLS)
/**
 * Callback function for mbedtls that outputs the mbedtls logging statements using our own log
 * callbacks.
 **/
static void mbedtlsOpenRVClientDebug(void* user, int level, const char* file, int line, const char* msg)
{
    orv_context_t* context = (orv_context_t*)user;
    if (level >= 2) {
        ORV_DEBUG(context, "mbedtls message (%d): %s (%s:%d)", level, msg, file, line);
    }
    else {
        ORV_INFO(context, "mbedtls message (%d): %s (%s:%d)", level, msg, file, line);
    }
}
#endif // OPENRV_HAVE_MBEDTLS


/**
 * The array of all tunnel capabilities known to this library.
 **/
static const orv_vnc_tight_capability_t gRegisteredTightTunnelCapabilities[] =
{
    orv_vnc_tight_capability_t{0, "TGHT", "NOTUNNEL"},
};

static const uint32_t gRegisteredTightTunnelCapabilitiesCount = sizeof(gRegisteredTightTunnelCapabilities) / sizeof(orv_vnc_tight_capability_t);

/**
 * @return The index of @p tunnel if in the @ref gRegisteredTightAuthCapabilities array, or -1 if
 *         @p tunnel is not a registered tunnel capability.
 *         A tunnel capability that is not registered cannot be used by this library.
 **/
static int findRegisteredTunnelIndex(const orv_vnc_tight_capability_t* tunnel)
{
    for (int registeredIndex = 0; registeredIndex < (int)gRegisteredTightTunnelCapabilitiesCount; registeredIndex++) {
        const orv_vnc_tight_capability_t& reg = gRegisteredTightTunnelCapabilities[registeredIndex];
        if (orv_are_capabilities_equal(tunnel, &reg)) {
            return registeredIndex;
        }
    }
    return -1;
}

/**
 * @pre The @p tunnel is in @ref gRegisteredTightTunnelCapabilities (otherwise results of this
 *      function are undefined)
 * @return 1 if this library supports the tunnel capability @p tunnel, otherwise 0.
 **/
static uint8_t isRegisteredTunnelSupportedByLibrary(const orv_vnc_tight_capability_t* tunnel)
{
    // all registered tunnel capabilities are currently supported.
    UNUSED(tunnel);
    return 1;
}

/**
 * The array of all authentication capabilities (in the "Tight" security type) known to this library
 * (supported or otherwise).
 *
 * See https://github.com/rfbproto for a list of registered types.
 **/
static const orv_vnc_tight_capability_t gRegisteredTightAuthCapabilities[] =
{
    orv_vnc_tight_capability_t{(int32_t)TightAuthType::None, "STDV", "NOAUTH__"},
    orv_vnc_tight_capability_t{(int32_t)TightAuthType::VNCAuthentication, "STDV", "VNCAUTH_"},
    orv_vnc_tight_capability_t{(int32_t)TightAuthType::VeNCrypt, "VENC", "VENCRYPT"},
    orv_vnc_tight_capability_t{(int32_t)TightAuthType::GTK_VNC_SASL, "GTKV", "SASL____"},
    orv_vnc_tight_capability_t{(int32_t)TightAuthType::UnixLoginAuthentication, "TGHT", "ULGNAUTH"}, // Unix Login Authentication // NOTE: no equivalent in the SecurityType types
    orv_vnc_tight_capability_t{(int32_t)TightAuthType::ExternalAuthentication, "TGHT", "XTRNAUTH"}, // External Authentication   // NOTE: no equivalent in the SecurityType types
};
static const uint32_t gRegisteredTightAuthCapabilitiesCount = sizeof(gRegisteredTightAuthCapabilities) / sizeof(orv_vnc_tight_capability_t);

/**
 * @return The index of @p authType if in the @ref gRegisteredTightAuthCapabilities array, or -1 if
 *         @p authType is not a registered auth type.
 *         An auth type that is not registered cannot be used by this library.
 **/
static int findRegisteredAuthTypeIndex(const orv_vnc_tight_capability_t* authType)
{
    for (int registeredIndex = 0; registeredIndex < (int)gRegisteredTightAuthCapabilitiesCount; registeredIndex++) {
        const orv_vnc_tight_capability_t& reg = gRegisteredTightAuthCapabilities[registeredIndex];
        if (orv_are_capabilities_equal(authType, &reg)) {
            return registeredIndex;
        }
    }
    return -1;
}

/**
 * @pre The @p authType is in @ref gRegisteredTightAuthCapabilities (otherwise results of this
 *      function are undefined)
 * @return 1 if this library supports the auth capability @p tunnel, otherwise 0.
 **/
uint8_t isRegisteredAuthCapabilitySupportedByLibrary(const orv_vnc_tight_capability_t* authType)
{
    switch (authType->mCode) {
        case (int32_t)TightAuthType::None:
        case (int32_t)TightAuthType::VNCAuthentication:
            return 1;
        case (int32_t)TightAuthType::Invalid:
        case (int32_t)TightAuthType::VeNCrypt:
        case (int32_t)TightAuthType::GTK_VNC_SASL:
        case (int32_t)TightAuthType::UnixLoginAuthentication:
        case (int32_t)TightAuthType::ExternalAuthentication:
            return 0;
        default:
            break;
    }
    return 0;
}

#define ORV_MAX_TUNNELS_COUNT 1000    // NOTE: exceeds realistic maximum a *lot*. Merely meant to limit network usage in case of errors.
#define ORV_MAX_AUTH_TYPES_COUNT 1000 // NOTE: exceeds realistic maximum a *lot*. Merely meant to limit network usage in case of errors.

SecurityTypeHandler::SecurityTypeHandler(struct orv_context_t* context, Socket* socket, const char* password, size_t passwordLength)
    : mContext(context),
      mSocket(socket),
      mPassword(password),
      mPasswordLength(passwordLength)
{
}

SecurityTypeHandlerNone::SecurityTypeHandlerNone(struct orv_context_t* context, Socket* socket)
    : SecurityTypeHandler(context, socket, nullptr, 0)
{
}

void SecurityTypeHandlerNone::handle(orv_error_t* error)
{
    UNUSED(error);
    // nothing to do for "None" security type
}

SecurityTypeHandlerVNCAuthentication::SecurityTypeHandlerVNCAuthentication(struct orv_context_t* context, Socket* socket, const char* password, size_t passwordLength)
    : SecurityTypeHandler(context, socket, password, passwordLength)
{
}

void SecurityTypeHandlerVNCAuthentication::handle(orv_error_t* error)
{
    const size_t challengeLength = 16;
    uint8_t challenge[challengeLength];
    uint8_t response[challengeLength];
    if (!mSocket->readDataBlocking(challenge, challengeLength, error)) {
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return;
        }
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 101, "Failed to read %u bytes challenge message for VNCAuthentication.", (unsigned int)challengeLength);
        return;
    }
    const char* password = mPassword;
    size_t passwordLength = mPasswordLength;
    if (!password) {
        password = "";
        passwordLength = 0;
    }
    if (!VncDES::encrypt(response, challenge, password, passwordLength)) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Internal error: Failed to encrypt challenge with DES");
        return;
    }
    if (!mSocket->writeDataBlocking(response, 16, error)) {
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return;
        }
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 18, "Failed to write VNC authentication challenge response to socket");
        return;
    }
}

SecurityTypeHandlerTight::SecurityTypeHandlerTight(struct orv_context_t* context, Socket* socket, const char* password, size_t passwordLength)
    : SecurityTypeHandler(context, socket, password, passwordLength)
{
}

SecurityTypeHandlerTight::~SecurityTypeHandlerTight()
{
    clear();
}

void SecurityTypeHandlerTight::clear()
{
    mReportedNumberOfTunnels = 0;
    free(mReportedTunnelCapabilities);
    mReportedTunnelCapabilities = nullptr;
    mSelectedTunnelCapabilityCode = 0;

    mReportedNumberOfAuthTypes = 0;
    free(mReportedAuthCapabilities);
    mReportedAuthCapabilities = nullptr;
    mSelectedAuthCapabilityCode = 0;
}

void SecurityTypeHandlerTight::handle(orv_error_t* error)
{
    clear();
    char numberOfTunnelsBuffer[4];
    if (!mSocket->readDataBlocking(numberOfTunnelsBuffer, 4, error)) {
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return;
        }
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 201, "Failed to read NumberOfTunnels for Tight authentication.");
        return;
    }
    mReportedNumberOfTunnels = Reader::readUInt32(numberOfTunnelsBuffer);
    handleTunnels(error);
    if (error->mHasError) {
        return;
    }
    handleAuthTypes(error);
    if (error->mHasError) {
        return;
    }
}

/**
 * @post On success, @ref mReportedNumberOfTunnels is set
 * @post On success, @ref mReportedTunnelCapabilities is set
 * @post On success, @ref mSelectedTunnelCapabilityCode is set
 * @pre @ref mReportedNumberOfTunnels has been properly initialized
 **/
void SecurityTypeHandlerTight::handleTunnels(orv_error_t* error)
{
    ORV_DEBUG(mContext, "Server reports %u tunnels (in tight security type)", (unsigned int)mReportedNumberOfTunnels);
    if (mReportedNumberOfTunnels > ORV_MAX_TUNNELS_COUNT) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 202, "Server supports %u tunnels, this client implementation allows at most %u.", (unsigned int)mReportedNumberOfTunnels, (unsigned int)ORV_MAX_TUNNELS_COUNT);
        return;
    }
    free(mReportedTunnelCapabilities);
    mReportedTunnelCapabilities = (orv_vnc_tight_capability_t*)malloc(std::max((size_t)(sizeof(orv_vnc_tight_capability_t) * mReportedNumberOfTunnels), (size_t)1));
    bool isRegisteredTunnelSupportedByServer[gRegisteredTightTunnelCapabilitiesCount] = {};
    int supportedRegisteredTunnels[gRegisteredTightTunnelCapabilitiesCount] = {}; // tunnels supported by library AND server // NOTE: in order of (server-)preference
    int supportedRegisteredTunnelsCount = 0;
    const size_t bytesPerTunnel = 16;
    std::vector<char> allTunnelsBuffer(mReportedNumberOfTunnels * bytesPerTunnel);
    if (!mSocket->readDataBlocking(allTunnelsBuffer.data(), mReportedNumberOfTunnels * bytesPerTunnel, error)) {
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return;
        }
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 203, "Failed to read tunnels from server, expected %u bytes.", (unsigned int)(mReportedNumberOfTunnels*bytesPerTunnel));
        return;
    }
    for (int i = 0; i < (int)mReportedNumberOfTunnels; i++) {
        const char* tunnelBuffer = allTunnelsBuffer.data() + i * bytesPerTunnel;
        orv_vnc_tight_capability_t& tunnel = mReportedTunnelCapabilities[i];
        tunnel.mCode = Reader::readInt32(tunnelBuffer + 0);
        memcpy(tunnel.mVendor, tunnelBuffer + 4, 4);
        memcpy(tunnel.mSignature, tunnelBuffer + 8, 8);
        tunnel.mVendor[4] = '\0';
        tunnel.mSignature[8] = '\0';
        ORV_DEBUG(mContext, "Server reported tunnel capability %d (vendor: %s, signature: %s)", (int)tunnel.mCode, tunnel.mVendor, tunnel.mSignature);
        int registeredIndex = findRegisteredTunnelIndex(&tunnel);
        if (registeredIndex >= 0) {
            // ensure server reported this tunnel capability only once.
            // prevents duplicates (and thus possible array overflow) in
            // supportedRegisteredTunnels array.
            if (!isRegisteredTunnelSupportedByServer[registeredIndex]) {
                isRegisteredTunnelSupportedByServer[registeredIndex] = true;
                if (isRegisteredTunnelSupportedByLibrary(&tunnel)) {
                    supportedRegisteredTunnels[supportedRegisteredTunnelsCount] = registeredIndex,
                    supportedRegisteredTunnelsCount++;
                }
            }
        }
    }
    if (mReportedNumberOfTunnels > 0) {
        if (supportedRegisteredTunnelsCount == 0) {
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 204, "Server reported %u tunnel capabilities, but none of them is supported by this client.", (unsigned int)mReportedNumberOfTunnels);
            return;
        }
        // select first tunnel capability that is registered (and thus supported)
        const int selectedTunnelCapabilityIndexInRegisteredArray = supportedRegisteredTunnels[0];
        const orv_vnc_tight_capability_t& selectedTunnelCapability = gRegisteredTightTunnelCapabilities[selectedTunnelCapabilityIndexInRegisteredArray];
        mSelectedTunnelCapabilityCode = selectedTunnelCapability.mCode;
        char buffer[4];
        Writer::writeInt32(buffer, selectedTunnelCapability.mCode);
        if (!mSocket->writeDataBlocking(buffer, 4, error)) {
            if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
                return;
            }
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 205, "Failed to select tunnel capability (%d), could not write 4 bytes to socket.", (int)selectedTunnelCapability.mCode);
            return;
        }
    }
    else {
        mSelectedTunnelCapabilityCode = 0; // NOTUNNEL (implicitly selected)
    }
}

/**
 * @post On success, @ref mReportedNumberOfAuthTypes is set
 * @post On success, @ref mReportedAuthCapabilities is set
 * @post On success, @ref mSelectedAuthCapabilityCode is set
 **/
void SecurityTypeHandlerTight::handleAuthTypes(orv_error_t* error)
{
    char numberOfAuthTypesBufffer[4];
    if (!mSocket->readDataBlocking(numberOfAuthTypesBufffer, 4, error)) {
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return;
        }
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 206, "Failed to read NumberOfAuthTypes for Tight authentication.");
        return;
    }
    mReportedNumberOfAuthTypes = Reader::readUInt32(numberOfAuthTypesBufffer);
    ORV_DEBUG(mContext, "Server reports %u auth types (in tight security type)", (unsigned int)mReportedNumberOfAuthTypes);
    if (mReportedNumberOfAuthTypes > ORV_MAX_AUTH_TYPES_COUNT) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 207, "Server provides %u auth types (in Tight security type), this client implementation allows at most %u.", (unsigned int)mReportedNumberOfAuthTypes, (unsigned int)ORV_MAX_AUTH_TYPES_COUNT);
        return;
    }
    free(mReportedAuthCapabilities);
    mReportedAuthCapabilities = (orv_vnc_tight_capability_t*)malloc(sizeof(orv_vnc_tight_capability_t) * mReportedNumberOfAuthTypes);
    bool isRegisteredAuthTypeSupportedByServer[gRegisteredTightAuthCapabilitiesCount] = {};
    int supportedRegisteredAuthTypes[gRegisteredTightAuthCapabilitiesCount] = {}; // auth types supported by library AND server // NOTE: in order of (server-)preference
    int supportedRegisteredAuthTypesCount = 0;
    const size_t bytesPerAuthType = 16;
    std::vector<char> allAuthTypesBuffer(mReportedNumberOfAuthTypes * bytesPerAuthType);
    if (!mSocket->readDataBlocking(allAuthTypesBuffer.data(), mReportedNumberOfAuthTypes * bytesPerAuthType, error)) {
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return;
        }
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 208, "Failed to read auth types (in tight security type) from server, expected %u bytes.", (unsigned int)(mReportedNumberOfAuthTypes*bytesPerAuthType));
        return;
    }
    for (int i = 0; i < (int)mReportedNumberOfAuthTypes; i++) {
        const char* authTypeBuffer = allAuthTypesBuffer.data() + i * bytesPerAuthType;
        orv_vnc_tight_capability_t& authType = mReportedAuthCapabilities[i];
        authType.mCode = Reader::readInt32(authTypeBuffer + 0);
        memcpy(authType.mVendor, authTypeBuffer + 4, 4);
        memcpy(authType.mSignature, authTypeBuffer + 8, 8);
        authType.mVendor[4] = '\0';
        authType.mSignature[8] = '\0';
        ORV_DEBUG(mContext, "  Server supports auth type: %d (vendor: %s signature: %s)", (int)authType.mCode, authType.mVendor, authType.mSignature);
        int registeredIndex = findRegisteredAuthTypeIndex(&authType);
        if (registeredIndex >= 0) {
            // ensure server reported this auth capability only once.
            // prevents duplicates (and thus possible array overflow) in
            // supportedRegisteredAuthTypes array.
            if (!isRegisteredAuthTypeSupportedByServer[registeredIndex]) {
                isRegisteredAuthTypeSupportedByServer[registeredIndex] = true;
                if (isRegisteredAuthCapabilitySupportedByLibrary(&authType)) {
                    supportedRegisteredAuthTypes[supportedRegisteredAuthTypesCount] = registeredIndex,
                    supportedRegisteredAuthTypesCount++;
                }
            }
        }
    }
    if (mReportedNumberOfAuthTypes > 0) {
        if (supportedRegisteredAuthTypesCount == 0) {
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 209, "Server reported %u auth capabilities, but none of them is supported by this client.", (unsigned int)mReportedNumberOfAuthTypes);
            return;
        }
        // select first auth capability from server list that is registered and supported by this 
        // library as well
        const int selectedAuthCapabilityIndexInRegisteredArray = supportedRegisteredAuthTypes[0];
        const orv_vnc_tight_capability_t& selectedAuthCapability = gRegisteredTightAuthCapabilities[selectedAuthCapabilityIndexInRegisteredArray];
        mSelectedAuthCapabilityCode = selectedAuthCapability.mCode;
        char buffer[4];
        Writer::writeInt32(buffer, selectedAuthCapability.mCode);
        if (!mSocket->writeDataBlocking(buffer, 4, error)) {
            if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
                return;
            }
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 210, "Failed to select auth capability (%d), failed to write 4 bytes to socket.", (int)selectedAuthCapability.mCode);
            return;
        }
    }
    else {
        mSelectedAuthCapabilityCode = (int32_t)SecurityType::None; // implicitly selected
    }

    if (error->mHasError) {
        return;
    }

    switch (mSelectedAuthCapabilityCode) {
        case (int32_t)SecurityType::None:
        {
            ORV_DEBUG(mContext, "Tight SecurityType: Selected None auth type");
            SecurityTypeHandlerNone handler(mContext, mSocket);
            handler.handle(error);
            break;
        }
        case (int32_t)SecurityType::VNCAuthentication:
        {
            ORV_DEBUG(mContext, "Tight SecurityType: Selected VNCAuthentication auth type");
            SecurityTypeHandlerVNCAuthentication handler(mContext, mSocket, mPassword, mPasswordLength);;
            handler.handle(error);
            break;
        }
        case (int32_t)SecurityType::VeNCrypt:
        case (int32_t)SecurityType::GTK_VNC_SASL:
        case 129: // ULGNAUTH (Unix Login Authentication)
        case 130: // XTRNAUTH (External Authentication)
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 211, "Tight authentication type %d selected, but not supported by this client.", (int)mSelectedAuthCapabilityCode);
            return;
        default:
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 212, "Tight authentication type %d selected, but not known by this client.", (int)mSelectedAuthCapabilityCode);
            return;
    }
}

bool SecurityTypeHandlerTight::selectedAuthTypeNone() const
{
    if (mSelectedAuthCapabilityCode == (int32_t)SecurityType::None) {
        return true;
    }
    return false;
}

#if defined(OPENRV_HAVE_MBEDTLS)
SecurityTypeHandlerVeNCrypt::SecurityTypeHandlerVeNCrypt(struct orv_context_t* context, Socket* socket, const char* password, size_t passwordLength, MbedTlsContext* mbedTlsContext)
    : SecurityTypeHandler(context, socket, password, passwordLength),
      mMbedTlsContext(mbedTlsContext)
{
}

SecurityTypeHandlerVeNCrypt::~SecurityTypeHandlerVeNCrypt()
{
    clear();
}

void SecurityTypeHandlerVeNCrypt::handle(orv_error_t* error)
{
    if (!mMbedTlsContext) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Internal error: NULL MbedTlsContext object for SecurityTypeHandlerVeNCrypt");
        return;
    }
    if (!mMbedTlsContext->init()) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Failed to initialize MbedTlsContext object");
        return;
    }

    //mbedtls_debug_set_threshold(1000);
    mbedtls_ssl_config conf;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    int ret = mbedtls_ssl_config_defaults( &conf,
                    MBEDTLS_SSL_IS_CLIENT,
                    MBEDTLS_SSL_TRANSPORT_STREAM,
                    MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Failed set mbedtls config defaults");
#warning TODO
        // TODO: free mbedtls resources
        return;
    }
    mbedtls_ssl_conf_dbg(&conf, mbedtlsOpenRVClientDebug, mContext);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, mMbedTlsContext->ctrDrbgContext());

#warning FIXME should NOT be used by default, disables certifcate checks
    //mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);

    ret = mbedtls_ssl_setup(&ssl, &conf);
    if (ret != 0) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Failed setup mbedtls ssl context");
#warning TODO
        // TODO: free mbedtls resources
        return;
    }

#warning TODO
    const char* hostname = "TODO";
    ret = mbedtls_ssl_set_hostname(&ssl, hostname);
    if (ret != 0) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Failed set mbedtls hostname");
#warning TODO
        // TODO: free mbedtls resources
        return;
    }

#if 1
    // TODO: use custom functions (with mContext as parameter) instead of mbedtls_net_context.
    mbedtls_net_context net;
    mbedtls_net_init(&net);
    net.fd = mSocket->socketFd();
    mbedtls_ssl_set_bio(&ssl, &net, mbedtls_net_send, mbedtls_net_recv, nullptr);
#endif

    ORV_DEBUG(mContext, "Performing TLS handshake using mbedtls...");
    do
    {
        ret = mbedtls_ssl_handshake(&ssl);
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            size_t bufSize = 1024;
            char buf[bufSize + 1];
            buf[bufSize] = 0;
            mbedtls_strerror(ret, buf, bufSize);
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "TLS handshake failed, mbedtls_ssl_handshake return value: -0x%x (%s)", -ret, buf);
#warning TODO
            // TODO: free mbedtls resources
            return;
        }
    } while (ret != 0);
    ORV_DEBUG(mContext, "TLS handshake finished successfully");

#if 0
    mbedtls_x509_crt cacert;
    mbedtls_x509_crt_init(&cacert);
#endif
    // TODO:
    // https://tls.mbed.org/kb/how-to/mbedtls-tutorial
}

void SecurityTypeHandlerVeNCrypt::clear()
{
}

#endif // OPENRV_HAVE_MBEDTLS

#if defined(OPENRV_HAVE_OPENSSL)
SecurityTypeHandlerAnonymousTLS::SecurityTypeHandlerAnonymousTLS(struct orv_context_t* context, Socket* socket, const char* password, size_t passwordLength, OpenSSLContext* openSSLContext)
    : SecurityTypeHandler(context, socket, password, passwordLength),
      mOpenSSLContext(openSSLContext)
{
}

SecurityTypeHandlerAnonymousTLS::~SecurityTypeHandlerAnonymousTLS()
{
    clear();
}

void SecurityTypeHandlerAnonymousTLS::handle(orv_error_t* error)
{
    /*
     * NOTE: This SecurityType is used by the "vino" VNC server.
     *       It implements *anonymous* TLS and is therefore utterly insecure (no protection at all
     *       against man in the middle attacks).
     *       vino uses gnutls as TLS library, according to gnutls debugging statements, the
     *       following are the supported algorithms:
     *           ANON_DH_ARCFOUR_MD5 (ID: 00018)
     *           ANON_DH_3DES_EDE_CBC_SHA1 (0x001B)
     *           ANON_DH_AES_128_CBC_SHA1 (ID: 0x0034)
     *           ANON_DH_AES_256_CBC_SHA1 (ID: 0x003A)
     *           ANON_DH_CAMELLIA_128_CBC_SHA1 (ID: 0x0046)
     *           ANON_DH_CAMELLIA_256_CBC_SHA1 (ID: 0x0089)
     *           ANON_DH_AES_128_CBC_SHA256 (ID: 0x006C)
     *           ANON_DH_AES_256_CBC_SHA256 (ID: 0x006D)
     *       mbedtls does NOT support any of these (probably intentionally, as anon tls is a bad
     *       idea).
     *
     *       mbedtls does not allow injecting custom algorithms (it uses
     *       mbedtls_ssl_ciphersuite_from_id() prior to sending the client hello, which looks up the
     *       ID in an array resides in private static memory). Consequently we cannot use mbedtls
     *       to support this SecurityType.
     */

    startOpenSSLHandshake(error);
    if (error->mHasError) {
        return;
    }

    mSocket->setEncryptionContext(mOpenSSLContext);

    // NOTE: The rest of this function is essentially repeating the whole "select security type"
    //       part of the RFB protocol.
    //       Except that no encryption sub-types make sense here.
    //       -> It may make sense to merge this code with the "normal" security type handling code,
    //          however error messages would get tricky (we want to give error messages about
    //          security sub-types here, not about security types), so it's probably not worth the
    //          effort.

    mSecuritySubTypesCount = 0;
    if (!mSocket->readDataBlocking(&mSecuritySubTypesCount, 1, error)) {
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return;
        }
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 0, "Failed to read security sub-types in anonymous TLS.");
        return;
    }
    if (mSecuritySubTypesCount == 0) {
        std::vector<uint8_t> reasonMessage;
        orv_error_t errorTmp;
        if (!Rfb3xHandshake::readReasonMessage(mSocket, &reasonMessage, &errorTmp)) {
            if (errorTmp.mErrorCode == ORV_ERR_USER_INTERRUPTION) {
                orv_error_copy(error, &errorTmp);
                return;
            }
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_SERVER_REJECTS_CONNECTION, 0, "Server rejected connection after anonymous TLS handshake (no sub-security types provided) and we failed reading reason message from server (details: %s)", errorTmp.mErrorMessage);
            return;
        }
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_SERVER_REJECTS_CONNECTION, 0, "Server rejected connection after anonymous TLS handshake (no sub-security types provided). Server provided reason: %s", reasonMessage.data());
        return;
    }

    if (!mSocket->readDataBlocking(mSecuritySubTypesSupportedByServer, (size_t)mSecuritySubTypesCount, error)) {
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return;
        }
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 0, "Failed to read %u security sub-types from server after anonymous TLS handshake", (unsigned int)mSecuritySubTypesCount);
        return;
    }
    ORV_DEBUG(mContext, "AnonymousTLS: Have %d security sub-types", mSecuritySubTypesCount);
    bool securitySubTypeSupported[255] = {};
    for (int i = 0; i < (int)mSecuritySubTypesCount; i++) {
        uint8_t securitySubType = mSecuritySubTypesSupportedByServer[i];
        securitySubTypeSupported[(int)securitySubType] = true;
        ORV_DEBUG(mContext, "  Server supports security sub-type %d (%s)", (int)securitySubType, OrvVncClient::getSecurityTypeString((SecurityType)securitySubType));
    }

    // List of SecurityType values supported as sub-type in this implementation.
    // In order of preference.
    // NOTE: Encryption types make no sense as sub-types.
    //       Also note: Use of anonymous TLS is discouraged and only pretty much discontinued, so it
    //       is unlikely that we ever need to support additional sub-types.
    const SecurityType supportedSubTypes[] = {
        SecurityType::None,
        SecurityType::VNCAuthentication,
    };
    const int supportedSubTypesCount = sizeof(supportedSubTypes) / sizeof(SecurityType);
    mSelectedSecuritySubType = SecurityType::Invalid;
    for (int i = 0; i <  supportedSubTypesCount; i++) {
        if (securitySubTypeSupported[(int)supportedSubTypes[i]]) {
            mSelectedSecuritySubType = supportedSubTypes[i];
            break;
        }
    }
    if (mSelectedSecuritySubType == SecurityType::Invalid) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_AUTHENTICATION_FAILED, 0, "Anonymous TLS: Server does not support any authentication method (sub-security type) that this client supports.");
        return;
    }

    uint8_t selectedSecurityType = (uint8_t)mSelectedSecuritySubType;
    if (!mSocket->writeDataBlocking(&selectedSecurityType, 1, error)) {
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return;
        }
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 0, "Anonymous TLS: Failed to write security sub-type to server socket");
        return;
    }
    ORV_DEBUG(mContext, "Anonymous TLS: Selected security sub-type %d (%s)", (int)mSelectedSecuritySubType, OrvVncClient::getSecurityTypeString((SecurityType)mSelectedSecuritySubType));

    switch (mSelectedSecuritySubType) {
        case SecurityType::None:
        {
            SecurityTypeHandlerNone handler(mContext, mSocket);
            handler.handle(error);
            break;
        }
        case SecurityType::VNCAuthentication:
        {
            SecurityTypeHandlerVNCAuthentication handler(mContext, mSocket, mPassword, mPasswordLength);
            handler.handle(error);
            break;
        }
        default:
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Internal error: Negotiated security sub type %d for anonymous TLS that this client does not support", (int)mSelectedSecuritySubType);
            return;
    }

    if (error->mHasError) {
        return;
    }

    ORV_DEBUG(mContext, "Anonymous TLS: Security sub-type finished");

    // NOTE: Depending on security sub-type and protocol version, the caller may need to read the
    //       SecurityResult message now.
}

// NOTE: This function is meant to be re-used by other security types *if* we ever add another one
//       that uses OpenSSL (we normally prefer mbedtls though)
//       -> the function should then be moved to a more generic place (e.g. the OpenSSLContext)
/**
 * @pre The socket is already connected to the remote server and waiting for the SSL handshake to
 *      start.
 **/
void SecurityTypeHandlerAnonymousTLS::startOpenSSLHandshake(orv_error_t* error)
{
    if (!mOpenSSLContext) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Internal error: NULL OpenSSLContext object for SecurityTypeHandlerAnonymousTLS");
        return;
    }
    orv_error_t errorTmp;
    if (!mOpenSSLContext->init(mContext, &errorTmp)) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Failed to initialize OpenSSLContext object, details: %s", errorTmp.mErrorMessage);
        return;
    }
    if (!mOpenSSLContext->ssl()) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Internal error: OpenSSLContext initialized, but have NULL SSL pointer");
        return;
    }

    ORV_INFO(mContext, "Setting OpenSSL context to anonymous TLS, because the SecurityType negotiated with server requires this. Note that anonymous TLS is inherently insecure, man-in-the-middle attacks are always possible! This connection is *not* secure!");
    if (!mOpenSSLContext->makeAnonymousTLS()) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Failed to make an anonymous TLS OpenSSL context. Unable to connect using AnonymousTLS.");
        return;
    }
    ORV_DEBUG(mContext, "OpenSSL context initialized");
    if (SSL_set_fd(mOpenSSLContext->ssl(), mSocket->socketFd()) != 1) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Failed to set fd to SSL object");
        return;
    }
    mOpenSSLContext->outputOpensSSLErrorsAndClearQueue(mContext, "After setting fd on SSL pointer");

    while (true) {
        int ret = SSL_connect(mOpenSSLContext->ssl());
        mOpenSSLContext->outputOpensSSLErrorsAndClearQueue(mContext, "After SSL_connect()");
        if (ret == 1) {
            // success, handshake finished.
            ORV_DEBUG(mContext, "SSL handshake finished successfully.");
            return;
        }
        if (ret > 1) {
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_SSL_HANDSHAKE, 0, "Internal error with SSL handshake: SSL_connect() returned %d, which is an invalid SSL_connect() return value", ret);
            return;
        }
        if (ret == 0) {
            int err = SSL_get_error(mOpenSSLContext->ssl(), ret);
            if (err == SSL_ERROR_SYSCALL) {
                // ret==0 at this point, according to OpenSSL docs this means an invalid EOF was
                // received. This happens e.g. when connecting to vino but we fail to negotiate a
                // cipher.
                orv_error_set(error, ORV_ERR_CONNECT_ERROR_SSL_HANDSHAKE, err, "Failed to connect, SSL handshake failed (OpenSSL error code %d), unexpected EOF encountered.", err);
                STACK_OF(SSL_CIPHER)* sk = SSL_get_ciphers(mOpenSSLContext->ssl());
                int c = sk_SSL_CIPHER_num(sk);
                for (int i = 0; i < c; i++) {
                    const SSL_CIPHER* cipher = sk_SSL_CIPHER_value(sk, i);
                    if (cipher) {
                        ORV_ERROR(mContext, "xx %s", SSL_CIPHER_get_name(cipher));
                    }
                }
                ORV_ERROR(mContext, "xxx %s", SSL_get_cipher_list(mOpenSSLContext->ssl(), 0));
                return;
            }
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_SSL_HANDSHAKE, err, "Failed to connect, SSL handshake failed (clean shutdown during handshake), OpenSSL error code: %d", err);
            return;
        }
        // ret is negative
        int err = SSL_get_error(mOpenSSLContext->ssl(), ret);
        switch (err) {
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
            {
                const Socket::WaitType type = (err == SSL_ERROR_WANT_WRITE) ? Socket::WaitType::Write : Socket::WaitType::Read;
                const uint64_t timeoutSec = mSocket->socketTimeoutSeconds();
                const uint64_t timeoutUsec = 0;
                const bool useTimeout = true;
                int lastError = 0;
                Socket::WaitRet waitRet = mSocket->waitForSignal(timeoutSec, timeoutUsec, useTimeout, type, &lastError);
                switch (waitRet) {
                    case Socket::WaitRet::Signalled:
                        break;
                    case Socket::WaitRet::Timeout:
                        orv_error_set(error, ORV_ERR_CONNECT_ERROR_TIMEOUT, 0, "Timeout occurred during SSL handshake");
                        return;
                    case Socket::WaitRet::UserInterruption:
                        orv_error_set(error, ORV_ERR_USER_INTERRUPTION, 0, ORV_ERROR_MSG_USER_INTERRUPTION);
                        return;
                    case Socket::WaitRet::Error:
                        orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Error during SSL handshake while waiting for data, lastError=%d.", lastError);
                        return;
                }
                break;
            }
            case SSL_ERROR_NONE:
                orv_error_set(error, ORV_ERR_CONNECT_ERROR_SSL_HANDSHAKE, 0, "Internal error during SSL handshake: SSL_connect returned %d, but SSL_get_error claims SSL_ERROR_NONE", err);
                return;
            case SSL_ERROR_SSL:
                // additional errors are in the OpenSSL error queue (we have already dumped and
                // cleared it)
                orv_error_set(error, ORV_ERR_CONNECT_ERROR_SSL_HANDSHAKE, err, "Failed to connect, SSL handshake failed (error during handshake), OpenSSL reports error in the SSL library (%d), see error logs for details", err);
                return;
            default:
                orv_error_set(error, ORV_ERR_CONNECT_ERROR_SSL_HANDSHAKE, err, "Failed to connect, SSL handshake failed (error during handshake), return code: %d, OpenSSL error code: %d", ret, err);
                return;
        }
    }
}

void SecurityTypeHandlerAnonymousTLS::clear()
{
    mSecuritySubTypesCount = 0;
    memset(mSecuritySubTypesSupportedByServer, 0, 255);
    mSelectedSecuritySubType = SecurityType::Invalid;
}

#endif // OPENRV_HAVE_MBEDTLS


} // namespace vnc
} // namespace openrv

