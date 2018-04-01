#ifndef OPENRV_RFBTYPES_H
#define OPENRV_RFBTYPES_H

#include <stdint.h>

/**
 * @file rfbtypes.h
 *
 * Message type, encoding type and similar type definitions of the RFB protocol.
 **/

namespace openrv {
namespace vnc {

/**
 * Known versions of the RFB protocol.
 **/
enum class ProtocolVersion
{
    Unknown = -1,

    /**
     * RFB version 3.3.
     *
     * This is a legacy version that is not normally used anymore and may be completely untested.
     *
     * This version is normally used in one special case only: When a server rejects a connection
     * immediately, it may do so by using RFB 3.3 and sending the "Invalid" security type followed
     * by a reason string (e.g. "too many connection attempts").
     * This is often preferred by a server over a RFB 3.8 rejection for highest client
     * compatibility.
     **/
    RFB_3_3,

    /**
     * RFB version 3.7. Normally version 3.8 is used instead, this version is merely a legacy
     * version and may be completely untested.
     *
     * This version is mostly identical to RFB 3.3, but adds another handshake step:
     * The server sends a list of supported security types and the client selects the security
     * type. In 3.3, the server sent the chosen security type immediately.
     **/
    RFB_3_7,

    /**
     * RFB version 3.8. This is the default 3.x version.
     *
     * This version is identical to 3.7 but adds
     * - Reason message (4 bytes length followed by string) after SecurityResult that indicated
     *   failure (3.3 and 3.7 simply closed the connection after failure SecurityResult).
     * - SecurityResult message if "None" security type was selected (3.3 and 3.7 directly continued
     *   with initialization phase instead, the protocol always assumed success on "None" security
     *   type).
     **/
    RFB_3_8,
};
/**
 * Security types, as defined by the RFB protocol.
 *
 * The values in this enum match the value specified by the RFB protocol.
 *
 * See also
 * - http://www.realvnc.com/docs/rfbproto.pdf
 * - https://www.iana.org/assignments/rfb/rfb.xhtml
 * for an official list of registered types for the RFB protocol.
 **/
enum class SecurityType
{
    Invalid                   = 0,
    None                      = 1,
    VNCAuthentication         = 2,
    RA2                       = 5,
    RA2ne                     = 6,
    Tight                     = 16,
    Ultra                     = 17,
    AnonymousTLS              = 18, /* Referred to as "TLS" in the RFB docs. This is a strictly anonymous TLS, not secured with any certificates and therefore easily attacked by man-in-the-middle attacks. Should be avoided whenever possible (use VeNCrypt instead). Used by "vino" of the GNOME project. */
    VeNCrypt                  = 19,
    GTK_VNC_SASL              = 20,
    MD5HashAuthentication     = 21,
    ColinDeanxvp              = 22,


    LastEntry // must remain LAST entry of enum
};
static_assert((int)SecurityType::LastEntry <= 256, "All SecurityType values must fit into a uint8_t according to the RFB protocol");

/**
 * Encodings that can be announced to the server. This includes "pseudo encodings", which merely
 * announce supported features to the server.
 *
 * The values registered at RealVNC, see also http://www.realvnc.com/docs/rfbproto.pdf
 *
 * Known values can be obtained from
 * - http://www.realvnc.com/docs/rfbproto.pdf (Official document, however non-standard values
 *   are not described in detail, often not even with a name)
 * - https://github.com/rfbproto (community maintained RFB spec, most encodings are also
 *   described here)
 * - https://www.iana.org/assignments/rfb/rfb.xhtml
 **/
enum class EncodingType : int32_t
{
    // "Standard" VNC encoding types (defined in http://www.realvnc.com/docs/rfbproto.pdf)
    /**
     * Raw pixel data
     *
     * NOTE: Server may always use this, even if not announced by client.
     **/
    Raw = 0,                                               // 0x00000000
    CopyRect = 1,                                          // 0x00000001
    RRE = 2,                                               // 0x00000002
    Hextile = 5,                                           // 0x00000005
    ZRLE = 16,                                             // 0x00000010
    Cursor= -239,                       // pseudo-encoding // 0xFFFFFF11
    DesktopSize= -223,                  // pseudo-encoding // 0xFFFFFF21

    // Additional registered encoding types
    CoRRE = 4,                                             // 0x00000004
    zlib = 6,                                              // 0x00000006
    tight = 7,                                             // 0x00000007
    zlibhex = 8,                                           // 0x00000008
    TRLE = 15,                                             // 0x0000000F
    HitachiZYWRLE = 17,                                    // 0x00000011
    AdamWallingXZ = 18,                                    // 0x00000012
    AdamWallingXZYW = 19,                                  // 0x00000013
    JPEG = 21,                                             // 0x00000015
    JRLE = 22,                                             // 0x00000016
    // -1 ... -222: ??? (no name given in protocol pdf)
    // jpeg quality level in tight encoding: -23..-32
    TightJpegQualityLevel9 = -23,       // pseudo-encoding // 0xFFFFFFE9
    TightJpegQualityLevelHigh = TightJpegQualityLevel9,
    TightJpegQualityLevel8 = -24,       // pseudo-encoding // 0xFFFFFFE8
    TightJpegQualityLevel7 = -25,       // pseudo-encoding // 0xFFFFFFE7
    TightJpegQualityLevel6 = -26,       // pseudo-encoding // 0xFFFFFFE6
    TightJpegQualityLevel5 = -27,       // pseudo-encoding // 0xFFFFFFE5
    TightJpegQualityLevel4 = -28,       // pseudo-encoding // 0xFFFFFFE4
    TightJpegQualityLevel3 = -29,       // pseudo-encoding // 0xFFFFFFE3
    TightJpegQualityLevel2 = -30,       // pseudo-encoding // 0xFFFFFFE2
    TightJpegQualityLevel1 = -31,       // pseudo-encoding // 0xFFFFFFE1
    TightJpegQualityLevel0 = -32,       // pseudo-encoding // 0xFFFFFFE0
    TightJpegQualityLevelLow = TightJpegQualityLevel0,
    // -224 ... -238: ??? (no name given in protocol pdf)
    LastRect = -224,                    // pseudo-encoding // 0xFFFFFF20
    PointerPosition = -232,             // pseudo-encoding // 0xFFFFFF18
    // -240 ... -256: tight options
    XCursor = -240,                     // pseudo-encoding // 0xFFFFFF10
    TightCompressionLevel = -256,       // pseudo-encoding // 0xFFFFFF00
    // -257 ... -272: Anthony Liguori
    // -273 ... -304: VMWare
    gii = -305,                                            // 0xFFFFFECF
    popa = -306,                                           // 0xFFFFFECE
    PeterAstrandDesktopName = -307,                        // 0xFFFFFECD
    PierreOssmanExtendedDesktopSize = -308,                // 0xFFFFFECC
    ColinDeanxvp = -309,                                   // 0xFFFFFECB
    OLIVECallControl = -310,                               // 0xFFFFFECA
    Fence = -312,                       // pseudo-encoding // 0xFFFFFEC8
    ContinuousUpdates = -313,           // pseudo-encoding // 0xFFFFFEC7
    CursorWithAlpha = -314,             // pseudo-encoding // 0xFFFFFEC6
    // -412 ... -512: TurboVNC fine-grained quality level
    JpegFineGrainedQualityLevel = -512, // pseudo-encoding // 0xFFFFFE00
    // -763 ... -768. TurboVNC subsampling level
    JpegSubsamplingLevel = -768,        // pseudo-encoding // 0xFFFFFD00
    // 0x574d5600 ... 0x574d56ff: VMWare
};


/**
 * The "auth" sub-type that is used when using @ref SecurityType::Tight as security type.
 *
 * The @ref SecurityType::Tight security type is a container security type that enables some
 * protocol extensions. The actual authentication is then performed using the "auth type".
 *
 * Note that an auth type is only valid if the "code" (i.e. the auth type defined here), the
 * "vendor" string and the "signature" string @em all match.
 *
 * See in particular https://github.com/rfbproto for a list of valid types.
 **/
enum class TightAuthType
{
    Invalid                 = 0,
    None                    = 1,   // NOTE: Same as SecurityType::None
    VNCAuthentication       = 2,   // NOTE: Same as SecurityType::VNCAuthentication
    VeNCrypt                = 19,  // NOTE: Same as SecurityType::VeNCrypt
    GTK_VNC_SASL            = 20,  // NOTE: Same as SecurityType::GTK_VNC_SASL
    UnixLoginAuthentication = 129,
    ExternalAuthentication  = 130,

    LastEntry // must remain LAST entry of enum
};
static_assert((int)TightAuthType::LastEntry <= 256, "All TightAuthType values must fit into a uint8_t according to the RFB protocol");

/**
 * Client to server messages
 **/
enum class ClientMessage : uint8_t
{
    // "Standard" VNC message types (defined in http://www.realvnc.com/docs/rfbproto.pdf)
    SetPixelFormat = 0,
    SetEncodings = 2,
    FramebufferUpdateRequest = 3,
    KeyEvent = 4,
    PointerEvent = 5,
    ClientCutText = 6,

    // Additional registered message types
    VMWare127 = 127,
    OLIVECallControl = 249,
    ColinDeanxvp = 250,
    PierreOssmanSetDesktopSize = 251,
    Tight = 252,
    Gii = 253,
    VMWare254 = 254,
    AnthonyLiguori = 255
};
/**
 * Server to client messages
 **/
enum class ServerMessage : uint8_t
{
    // "Standard" VNC message types (defined in http://www.realvnc.com/docs/rfbproto.pdf)
    FramebufferUpdate = 0,
    SetColourMapEntries = 1,
    Bell = 2,
    ServerCutText = 3,

    // Additional registered message types
    VMWare127 = 127,
    OLIVECallControl = 249,
    ColinDeanxvp = 250,
    Tight = 252,
    Gii = 253,
    VMWare254 = 254,
    AnthonyLiguori = 255
};

} // namespace vnc
} // namespace openrv

#endif

