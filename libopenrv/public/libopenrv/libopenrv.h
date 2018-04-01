#ifndef OPENRV_LIBOPENRV_H
#define OPENRV_LIBOPENRV_H

#include <libopenrv/orv_version.h>
#include <libopenrv/orv_config.h>
#include "orv_logging.h"
#include "orv_error.h"
#include "orv_errorcodes.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* TODO: add some kind of auto-update functionality that sends FramebufferUpdateRequest messages
         automatically n ms after the previous request has been finished */
/* TODO: add an "update counter" to the framebuffer and to framebuffer update events.
         when an event is received by the application, it normally acquires the fb and starts
         displaying it. however it is possible that multiple events are emitted in short succession,
         possibly the framebuffer already contains all updates of event n+1 when event n is received
         by the client - the update of the client for event n+1 may not be needed (assuming it
         either ignores the update rectangle or both events cover the same area). If we maintain an
         update counter, the client can easily notice that and make event n+1 a noop. */
/* TODO: maybe replace orv_connect_options_t by a orv_connect_parameters_t that ALSO holds
 * - hostname
 * - port
 * - password (MUST be NUL-terminated)
 * - passwordfilename (used if password is null)
 * - additional login data, if necessary for any security types (e.g. username)
 *
 * HOWEVER: username + password require a malloc()/free() call, which are problematic.
 *          if the user requires to perform it, they will often be forgotten.
 *          if we free() these values in orv_connect(), we have a somewhat inconvenient API
 *          (parameters are non-const and need to be set by the user again to the same values for
 *          another connection to the same host)
 */

/**
 * Maximum allowed length of host names, not including the terminating '\0' character. All buffers
 * holding a hostname must hold at least ORV_MAX_HOSTNAME_LEN+1 bytes, the last byte must always be
 * ’\0’.
 **/
#define ORV_MAX_HOSTNAME_LEN 255

/**
 * Maximum length of the desktop name (excluding the terminating NUL character) used by this
 * library.
 **/
#define ORV_MAX_DESKTOP_NAME_LENGTH 100*1024

/*#define ORV_MAX_VNC_MESSAGE_TYPE_STRING_LEN 100*/

/**
 * The library currently refuses to allocate buffer for texts larger than this value.
 *
 * VNC protocol requires text to be latin-1, so binary data (e.g. images) cannot be stored here
 * anyway.
 *
 * If a larger message arrives, the library reads up to this size and simply skips the remaining
 * data.
 **/
#define ORV_MAX_VNC_SERVER_CUT_TEXT_SIZE 2*1024*1024

#define ORV_MAX_PASSWORD_LEN 1024*1024*10

/**
 * The maximum width of a framebuffer that will be accepted from a server. If a framebuffer larger
 * than this is received from the server, the connection will be rejected/closed.
 *
 * Note that the memory requirements for a single framebuffer array is
 * framebufferWidth*framebufferHeight*framebufferBytesPerPixel.
 * (The total memory requirements will be larger, as additional buffers are required)
 **/
#define ORV_MAX_FRAMEBUFFER_WIDTH (1024*64)

/**
 * The maximum height of a framebuffer that will be accepted from a server. If a framebuffer larger
 * than this is received from the server, the connection will be rejected/closed.
 *
 * Note that the memory requirements for a single framebuffer array is
 * framebufferWidth*framebufferHeight*framebufferBytesPerPixel.
 * (The total memory requirements will be larger, as additional buffers are required)
 **/
#define ORV_MAX_FRAMEBUFFER_HEIGHT (1024*64)

/**
 * Maximum size of a framebuffer array (in bytes) that will be allocated by this library. If the
 * requested framebuffer size exceeds this value, the connection will be rejected/closed.
 **/
#define ORV_MAX_FRAMEBUFFER_MEMORY (1*1024*1024*1024)

/**
 * Maximum number of encodings that will be stored by this library, see @ref
 * orv_vnc_server_capabilities_t.
 *
 * If a server provides more encodings, the list of encoding type stored in @ref
 * orv_vnc_server_capabilities_t will be truncated.
 **/
#define ORV_MAX_VNC_ENCODING_TYPES_STORED_COUNT 100

/**
 * Similar to @ref ORV_MAX_VNC_ENCODING_TYPES_STORED_COUNT, but this value limits the
 * maximum number of capabilities @em read from the server in the initialization phase.
 *
 * This value is meant to ensure that the connection does not "run amok" if garbage is read for a
 * capabilities count.
 *
 * The value is chosen such that it exceeds any realistic maximum value a server might report by a
 * long shot.
 **/
#define ORV_MAX_VNC_ENCODING_TYPES_READ_COUNT 10000

/**
 * Maximum number of security types that will be read and stored by this library. Note that the
 * security type identifier is an 8 bit integer in the RFB protocol and therefore cannot exceed 255.
 **/
#define ORV_MAX_VNC_SECURITY_TYPES_COUNT 255

/**
 * The length of the version string used by the RFB/VNC protocol. Always 12 bytes, including a
 * terminating newline ('\n') character.
 **/
#define ORV_VNC_PROTOCOL_VERSION_STRING_LENGTH 12

/**
 * The maximum number of "server message" capabilities that are stored by this library, see @ref
 * orv_vnc_server_capabilities_t.
 * This is relevant for the "Tight" security type only, which includes a capabilities section in its
 * initialization phase, where the server reports what message types the server supports.
 *
 * If a server provides more message types, the list of stored message types will be truncated.
 **/
#define ORV_MAX_VNC_SERVER_MESSAGE_CAPABILITIES_STORED_COUNT 100

/**
 * Similar to @ref ORV_MAX_VNC_SERVER_MESSAGE_CAPABILITIES_STORED_COUNT, but this value limits the
 * maximum number of capabilities @em read from the server in the initialization phase.
 *
 * This value is meant to ensure that the connection does not "run amok" if garbage is read for a
 * capabilities count.
 *
 * The value is chosen such that it exceeds any realistic maximum value a server might report by a
 * long shot.
 **/
#define ORV_MAX_VNC_SERVER_MESSAGE_CAPABILITIES_READ_COUNT 10000

/**
 * The maximum number of "client message" capabilities that are stored by this library, see @ref
 * orv_vnc_server_capabilities_t.
 * This is relevant for the "Tight" security type only, which includes a capabilities section in its
 * initialization phase, where the server reports what message types the server supports.
 *
 * If a server provides more message types, the list of stored message types will be truncated.
 **/
#define ORV_MAX_VNC_CLIENT_MESSAGE_CAPABILITIES_STORED_COUNT 100

/**
 * Similar to @ref ORV_MAX_VNC_CLIENT_MESSAGE_CAPABILITIES_STORED_COUNT, but this value limits the
 * maximum number of capabilities @em read from the server in the initialization phase.
 *
 * This value is meant to ensure that the connection does not "run amok" if garbage is read for a
 * capabilities count.
 *
 * The value is chosen such that it exceeds any realistic maximum value a server might report by a
 * long shot.
 **/
#define ORV_MAX_VNC_CLIENT_MESSAGE_CAPABILITIES_READ_COUNT 10000

/**
 * Type that is used to reference user data.
 *
 * This is equivalent to a 0-based integer index.
 **/
typedef enum orv_user_data_t
{
    ORV_USER_DATA_0 = 0,
    ORV_USER_DATA_1,
    ORV_USER_DATA_2,
    ORV_USER_DATA_3,
    ORV_USER_DATA_4,

    ORV_USER_DATA_COUNT
} orv_user_data_t;

typedef enum orv_event_type_t
{
    /**
     * Type indicating that no event is available.
     **/
    ORV_EVENT_NONE,

    /**
     * Event indicating that the library finished connecting to a host. The event will provide data
     * of type orv_connect_result_t.
     **/
    ORV_EVENT_CONNECT_RESULT,
    /*ORV_EVENT_MESSAGE_SENT,*/ /* do we want this? may be too much overhead for PointerEvent/KeyEvent messages */
    ORV_EVENT_DISCONNECTED,
    ORV_EVENT_CUT_TEXT,
    ORV_EVENT_FRAMEBUFFER_UPDATED,
    /**
     * Event indicating that the responses (zero or more ORV_EVENT_FRAMEBUFFER_UPDATED events)
     * to orv_request_framebuffer_update() have been received,
     * the framebuffer update request has been completed. The client should normally issue another
     * update request as response to this event.
     *
     * This event has no event data.
     **/
    ORV_EVENT_FRAMEBUFFER_UPDATE_REQUEST_FINISHED,

    /**
     * Event indicating that the cursor data has changed. The current cursor data can be obtained
     * by @ref orv_acquire_cursor().
     *
     * This library requests the server to @em not provide the cursor in the framebuffer data, which
     * normally improves performance significantly. Instead, the server sends an updated cursor
     * shape whenever the shape changes and asks the client (= us) to draw the cursor itself on top
     * of the framebuffer. The implementing application is meant to draw the cursor provided by @ref
     * orv_acquire_cursor() at the current cursor position.
     *
     * Also note that this feature is normally supported by VNC servers, but this is not necessarily
     * always true. A server may instead provide the cursor directly in the framebuffer. In that
     * case, this event is never sent and the cursor returned by @ref orv_acquire_cursor() has the
     * @ref orv_cursor_t::mIsValid flag set to 0.
     *
     * This event has no event data.
     **/
    ORV_EVENT_CURSOR_UPDATED,

    /**
     * Event indicating that the server requests the client to ring a bell (if available).
     *
     * This event has no data associated.
     **/
    ORV_EVENT_BELL,

    /**
     * First event that is sent for a context/thread. This can be used by implementations of @ref
     * orv_context_t::mEventCallback to initialize thread-specific data.
     *
     * The @ref orv_poll_event() function will never returns this event.
     *
     * This event provides a NUL-terminated latin-1 string containing a name of the thread in the
     * event data. The name can be used for debugging.
     **/
    ORV_EVENT_THREAD_STARTED,

    /**
     * Last event that is sent for a context/thread. This can be used by implementations of @ref
     * orv_context_t::mEventCallback to clean up thread-specific data.
     *
     * The @ref orv_poll_event() function will never returns this event.
     *
     * This event provides a NUL-terminated latin-1 string containing a name of the thread in the
     * event data. The name can be used for debugging.
     **/
    ORV_EVENT_THREAD_ABOUT_TO_STOP,
} orv_event_type_t;

typedef struct orv_event_t
{
    orv_event_type_t mEventType;

    /**
     * Type-specific data for the event. The data is normally either NULL or a pointer to a struct
     * specific to the event type.
     **/
    void* mEventData;
} orv_event_t;

/**
 * Enum that defines what type of communication quality should be used, i.e. whether "fast transfer"
 * or "good image quality" should be preferred.
 *
 * This is mostly a convenience type that can be used instead of providing the @ref
 * orv_communication_pixel_format_t manually.
 **/
typedef enum orv_communication_quality_profile_t
{
    /**
     * Prefer fast transfer over good quality, i.e. use a low image quality.
     *
     * This uses 8 bit RGB for the communication (256 colors) and can be significantly faster than
     * the other values on slow network connections.
     **/
    ORV_COMM_QUALITY_PROFILE_LOW,
    /**
     * Use medium image quality.
     *
     * This uses 16 bit RGB for the communication.
     **/
    ORV_COMM_QUALITY_PROFILE_MEDIUM,
    /**
     * Use the best image quality, which requires the highest network bandwidth.
     *
     * This uses 24 bit RGB for the communication (8 bit per color channel), which is normally
     * identical to the color depth used by the server itself.
     **/
    ORV_COMM_QUALITY_PROFILE_BEST,
    /**
     * Use the @ref orv_communication_pixel_format_t values provided by the remote server.
     **/
    ORV_COMM_QUALITY_PROFILE_SERVER,

    /**
     * Use a custom @ref orv_communication_pixel_format_t value that is separately provided. See
     * @ref orv_connect_options_t.
     **/
    ORV_COMM_QUALITY_PROFILE_CUSTOM,

    /*
     * TODO: adaptive entry.
     * If set, we start with a bad quality, observe the transfer speed and attempt to increase
     * quality, if speed is good enough (and stay there as long as the speed stays good enough).
     */
} orv_communication_quality_profile_t;

const char* orv_get_communication_quality_profile_string(orv_communication_quality_profile_t qualityProfile);
orv_communication_quality_profile_t orv_get_communication_quality_profile_from_string(const char* string, orv_communication_quality_profile_t fallback);

/**
 * A struct providing information about the pixel format that is used in the communication with the
 * remote server.
 *
 * NOTE: The pixel format that is provided by this library in the framebuffer (see
 *       @ref orv_framebuffer_t) may differ from this: The data is extracted from the communication
 *       data internally to local buffers.
 *
 * This library sets the pixel format that is used for the communication automatically. You only
 * need this struct if you want to tweak the communication parameters for whatever reason.
 **/
// TODO: consider renaming this to orv_vnc_communication_pixel_format_t and add a separate struct
//       for non-vnc protocols.
//       several values are very vnc specific (such as mColorMax and mColorShift) and are likely
//       solved differently in different protocols.
//       alternatively, we may simply document that certain values are used for certain protocols
//       only.
typedef struct orv_communication_pixel_format_t
{
    /**
     * The number of bits per pixel in this format.
     *
     * NOTE: According to the RFB protocol, only the value 8, 16 and 32 are currently allowed.
     **/
    uint8_t mBitsPerPixel;
    /**
     * The depth of the format, that is the number of bits per pixel that are actually being used.
     **/
    uint8_t mDepth;
    /**
     * Boolean flag that indicates whether the framebuffer data uses big endian (1) or little endian
     * (0) representations of the data. Only the framebuffer data is influenced by this value and
     * only if @ref mBitsPerPixel is greater than 8.
     *
     * This value should normally be 0.
     **/
    uint8_t mBigEndian;
    /**
     * Boolean flag that indicates whether the "true color" mode should be used (1) or "color map"
     * mode should be used (0).
     *
     * Currently this library supports true color only, which is normally the preferred mode, so
     * this value should always be 1.
     *
     * If color map mode is used, the remaining entries in this struct (@ref mRedMax, @ref
     * mGreenMax, @ref mBlueMax, @ref mRedShift, @ref mGreenShift, @ref mBlueShift) are unused.
     **/
    uint8_t mTrueColor;
    /**
     * The maximum red/green/blue (in this order) value that can occur in the pixel data.
     * This is used to extract the color values.
     **/
    uint16_t mColorMax[3];
    /**
     * The number of bit (right-)shifts to reach the red/green/blue (in this order) value of a
     * pixel.
     * This is used to extract the color values.
     **/
    uint8_t mColorShift[3];
} orv_communication_pixel_format_t;

void orv_communication_pixel_format_reset(orv_communication_pixel_format_t* format);
void orv_communication_pixel_format_copy(orv_communication_pixel_format_t* dst, const orv_communication_pixel_format_t* src);

typedef struct orv_connection_info_t
{
    /* TODO: actually some data (hostname, port, received bytes, sent bytes) may also be valid if
       not yet full connected or if not connected anymore. check this and document accordingly. */
    uint8_t mConnected; /**< Boolean, 1 if connected, otherwise 0. Contents of this struct are only meaningful if this is 1. **/
    char mHostName[ORV_MAX_HOSTNAME_LEN + 1];
    uint16_t mPort;
    char mSelectedProtocolVersionString[ORV_VNC_PROTOCOL_VERSION_STRING_LENGTH + 1]; /**< Probably unused for non-VNC protocols **/
    uint8_t mSelectedVNCSecurityType;
    orv_communication_pixel_format_t mCommunicationPixelFormat;

    /**
     * The pixel format advertised by the server.
     *
     * NOTE: The client may have selected a different format. See @ref mCommunicationPixelFormat for
     *       the format that is currently used for the communication.
     **/
    orv_communication_pixel_format_t mDefaultPixelFormat;
    uint16_t mDefaultFramebufferWidth;
    uint16_t mDefaultFramebufferHeight;

    uint16_t mFramebufferWidth;
    uint16_t mFramebufferHeight;
    char mDesktopName[ORV_MAX_DESKTOP_NAME_LENGTH + 1];
    uint32_t mReceivedBytes;
    uint32_t mSentBytes;
} orv_connection_info_t;

/**
 * Struct that holds a "capability" of the server. This is primarily used when the "Tight" security
 * type is enabled, as the server can then report the capabilities it supports. However this struct
 * may also be used to report the capabilities that a server is known to support.
 **/
typedef struct orv_vnc_tight_capability_t
{
    int32_t mCode;
    uint8_t mVendor[5];    /* 4 byte vendor string plus NUL-termination */
    uint8_t mSignature[9]; /* 8 byte signature string plus NUL-termination */
} orv_vnc_tight_capability_t;

uint8_t orv_are_capabilities_equal(const orv_vnc_tight_capability_t* t1, const orv_vnc_tight_capability_t* t2);

/**
 * Information about the capabilities of a server. This is similar to @ref orv_connect_info_t, but
 * provides data that is less often queried. It often makes sense to update @ref orv_connect_info_t
 * regularly, but this makes rarely sense for the data provided here.
 *
 * If @ref mSupportedEncodingCapabilitiesPartial is 0, the data in @ref
 * mSupportedClientMessageCapabilities is complete
 * and was reported by the server. If @ref mSupportedEncodingCapabilitiesPartial is 1, then the data
 * may change while the connection is active, when the server reports support for an encoding.
 *
 * The @ref mSupportedEncodingCapabilitiesPartial is always 0 if the "Tight" security type is
 * reported, but will be 1 for the basic vnc security type(s), because the basic RFB/VNC protocol
 * does not provide a way for the server to report the set of supported encodings. In particular,
 * the client may advertise support for an encoding to the server and only find out if the server
 * supports that encoding once the server starts using it.
 *
 * NOTE: For encodings that have multiple quality levels, only a single level will be listed in the
 *       @ref mSupportedEncodingCapabilities array.
 **/
typedef struct orv_vnc_server_capabilities_t
{
    /**
     * The protocol version reported by the server. NOTE: The client may have selected a different
     * version.
     *
     * Always NUL-terminated.
     **/
    char mServerProtocolVersionString[13];
    int mServerProtocolVersionMajor;
    int mServerProtocolVersionMinor;
    orv_vnc_tight_capability_t mSupportedEncodingCapabilities[ORV_MAX_VNC_ENCODING_TYPES_STORED_COUNT];
    uint16_t mSupportedEncodingCapabilitiesCount;
    uint8_t mSupportedEncodingCapabilitiesPartial;
    uint8_t mSupportedSecurityTypes[ORV_MAX_VNC_SECURITY_TYPES_COUNT];
    uint8_t mSupportedSecurityTypesCount;
    orv_vnc_tight_capability_t mSupportedServerMessageCapabilities[ORV_MAX_VNC_SERVER_MESSAGE_CAPABILITIES_STORED_COUNT];
    uint32_t mSupportedServerMessageCapabilitiesCount;
    orv_vnc_tight_capability_t mSupportedClientMessageCapabilities[ORV_MAX_VNC_CLIENT_MESSAGE_CAPABILITIES_STORED_COUNT];
    uint32_t mSupportedClientMessageCapabilitiesCount;
} orv_vnc_server_capabilities_t;

void orv_connection_info_reset(orv_connection_info_t* info);
void orv_connection_info_copy(orv_connection_info_t* dst, const orv_connection_info_t* src);
void orv_connection_info_print_to_log(const struct orv_context_t* ctx, const orv_connection_info_t* capabilities);
void orv_vnc_server_capabilities_reset(orv_vnc_server_capabilities_t* capabilities);
void orv_vnc_server_capabilities_copy(orv_vnc_server_capabilities_t* dst, const orv_vnc_server_capabilities_t* src);
void orv_vnc_server_capabilities_print_to_log(const struct orv_context_t* ctx, const orv_vnc_server_capabilities_t* capabilities);
void orv_get_vnc_connection_info(const struct orv_context_t* ctx, orv_connection_info_t* info, orv_vnc_server_capabilities_t* capabilities);

/**
 * Data for ORV_EVENT_CONNECT_RESULT events.
 **/
typedef struct orv_connect_result_t
{
    char mHostName[ORV_MAX_HOSTNAME_LEN + 1];
    uint16_t mPort;
    orv_error_t mError;

    /**
     * If connection is successful (@ref mError has no error), this contains the framebuffer width
     * on the server.
     **/
    uint16_t mFramebufferWidth;
    /**
     * If connection is successful (@ref mError has no error), this contains the framebuffer height
     * on the server.
     **/
    uint16_t mFramebufferHeight;

    /**
     * The desktop name reported by the server, NUL-terminated. A non-NULL, NUL-terminated, empty
     * string if connection indicates error.
     *
     * The encoding of this string is undefined in the RFB protocol specification, but is normally
     * UTF-8 encoded in recent implementations.
     **/
    char* mDesktopName;

    /**
     * The pixel format that is used for the communication with the server by default.
     *
     * This value can be used to tweak the communication, if required. This value is @em NOT needed
     * to display the framebuffer. In fact, the framebuffer data provided by this library may @em
     * NOT match this format.
     **/
    orv_communication_pixel_format_t mCommunicationPixelFormat;
} orv_connect_result_t;

typedef struct orv_disconnected_t
{
    char mHostName[ORV_MAX_HOSTNAME_LEN + 1];
    uint16_t mPort;
    orv_error_t mError;
} orv_disconnected_t;

typedef struct orv_cut_text_t
{
    char* mText;
} orv_cut_text_t;

/**
 * Data for the @ref ORV_EVENT_FRAMEBUFFER_UPDATED event. The data describes what parts of the
 * framebuffer have been updated. The updated framebuffer can be queried using @ref
 * orv_acquire_framebuffer().
 **/
typedef struct orv_event_framebuffer_t
{
    uint16_t mX;
    uint16_t mY;
    uint16_t mWidth;
    uint16_t mHeight;
} orv_event_framebuffer_t;

orv_event_t* orv_event_init(orv_event_type_t type);
orv_event_t* orv_event_connect_result_init(const char* hostName, uint16_t port, uint16_t width, uint16_t height, const char* desktopName, const orv_communication_pixel_format_t* format, const orv_error_t* error);
orv_event_t* orv_event_disconnected_init(const char* hostName, uint16_t port, uint8_t gracefulExit, const orv_error_t* error);
orv_event_t* orv_event_framebuffer_init(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void orv_event_destroy(orv_event_t* event);
void orv_event_print_to_log(const struct orv_context_t* ctx, const orv_event_t* event);

typedef void (*orv_event_callback_t)(struct orv_context_t* ctx, orv_event_t* event);

/**
 * Implementation of an event callback that stores all events in an internal queue that can be
 * polled using orv_poll_event().
 **/
void orv_event_callback_polling(struct orv_context_t* ctx, orv_event_t* event);

typedef struct orv_config_t
{
    /**
     * Callback that receives internal logging information from the library.
     *
     * If set to NULL, no logging occurs.
     *
     * This function must be thread-safe, it may be called by different internal threads.
     **/
    orv_log_callback_t mLogCallback;

    /**
     * Function that is called whenever the library generates an event (such as "connected",
     * "disconnected", "new framebuffer data", ...)
     *
     * This function is called by an internal thread of the library, NOT by the thread that created
     * the context. Note however that the calling thread will @em always be the same thread for a
     * given function (but normally different threads for different contexts).
     *
     * This function is responsible for the data provided to it and must free the event after
     * using it. A minimal implementation of this callback has to at least free the event, even
     * if otherwise ignoring it (see also @ref orv_event_destroy()).
     **/
    orv_event_callback_t mEventCallback;
} orv_config_t;

void orv_config_zero(orv_config_t* cfg);
void orv_config_copy(orv_config_t* dst, const orv_config_t* src);
void orv_config_default(orv_config_t* cfg);


/**
 * Optional parameters to @ref orv_connect().
 **/
typedef struct orv_connect_options_t
{
    /**
     * Initial value of @ref orv_is_viewonly().
     **/
    uint8_t mViewOnly;

    /**
     * The quality profile for the communication.
     **/
    orv_communication_quality_profile_t mCommunicationQualityProfile;

    /**
     * If @ref mCommunicationQualityProfile is set to @ref ORV_COMM_QUALITY_PROFILE_CUSTOM, this
     * value is used for the communication.
     *
     * This is meant for advanced usage.
     **/
    struct orv_communication_pixel_format_t mCommunicationPixelFormat;
} orv_connect_options_t;

void orv_connect_options_default(orv_connect_options_t* options);


typedef struct orv_context_t orv_context_t;

orv_context_t* orv_init(const orv_config_t* cfg);
void orv_destroy(orv_context_t* ctx);

int orv_connect(orv_context_t* ctx, const char* host, uint16_t port, const char* password, const orv_connect_options_t* options, orv_error_t* error);
void orv_disconnect(orv_context_t* ctx);
int orv_is_connected(orv_context_t* ctx);

void orv_request_framebuffer_update(orv_context_t* ctx, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void orv_request_framebuffer_update_full(orv_context_t* ctx);

orv_event_t* orv_poll_event(orv_context_t* ctx);

void orv_set_user_data(orv_context_t* ctx, orv_user_data_t index, void* userData);
void orv_set_user_data_int(orv_context_t* ctx, orv_user_data_t index, int userData);
void* orv_get_user_data(orv_context_t* ctx, orv_user_data_t index);
int orv_get_user_data_int(orv_context_t* ctx, orv_user_data_t index);


const char* orv_get_vnc_client_message_type_string(uint8_t clientMessageType);
const char* orv_get_vnc_server_message_type_string(uint8_t serverMessageType);
const char* orv_get_vnc_security_type_string(uint8_t securityType);
const char* orv_get_vnc_encoding_type_string(int32_t encodingType);

typedef struct orv_framebuffer_t
{
    /**
     * Framebuffer array. The pixel are stored line-by-line (i.e. index 1 is x=1,y=0). Each pixel
     * currently has 3 bytes (RGB data).
     **/
    uint8_t* mFramebuffer;
    uint16_t mWidth;
    uint16_t mHeight;
    /**
     * The bits per pixel in @p mFramebuffer. Currently always 24 (RGB data).
     **/
    uint8_t mBitsPerPixel;
    /**
     * The value @ref mBitsPerPixel divided by 8, for convenience. Currently always 3 (RGB data).
     **/
    uint8_t mBytesPerPixel;
    /**
     * Total size of @ref mFramebuffer in bytes
     **/
    size_t mSize;
} orv_framebuffer_t;

typedef struct orv_cursor_t
{
    /**
     * 1 if a cursor was received from the server, otherwise 0. If 0, all other fields in this
     * struct are unused.
     **/
    uint8_t mIsValid;
    uint8_t* mCursor;
    uint16_t mWidth;
    uint16_t mHeight;
    uint16_t mHotspotX;
    uint16_t mHotspotY;
    /**
     * Number of bits per pixel. This always matches @ref mBytesPerPixel multiplied with 8.
     **/
    uint8_t  mBitsPerPixel;
    /**
     * Number of bytes per pixel in @ref mCursor.
     *
     * Currently always 4: RGBA data.
     **/
    uint8_t  mBytesPerPixel;
    /**
     * Total size of @ref mCursor in bytes
     **/
    uint32_t mCursorSize;
    /**
     * Internal variable that tracks the total array size of @ref mCursor, used to decide if the
     * array can be re-used or has to be re-allocated internally.
     *
     * The size that of @ref mCursor that is actually used is provided by @ref mCursorSize.
     **/
    uint32_t mCursorCapacity;
} orv_cursor_t;

typedef enum orv_mouse_button_flag_t
{
    ORV_MOUSE_BUTTON_NONE   = 0x00,
    ORV_MOUSE_BUTTON_1      = 0x01,
    ORV_MOUSE_BUTTON_LEFT   = ORV_MOUSE_BUTTON_1,
    ORV_MOUSE_BUTTON_2      = 0x02,
    ORV_MOUSE_BUTTON_MIDDLE = ORV_MOUSE_BUTTON_2,
    ORV_MOUSE_BUTTON_3      = 0x04,
    ORV_MOUSE_BUTTON_RIGHT  = ORV_MOUSE_BUTTON_3,
    ORV_MOUSE_BUTTON_4      = 0x08,
    ORV_MOUSE_WHEEL_UP      = ORV_MOUSE_BUTTON_4,
    ORV_MOUSE_BUTTON_5      = 0x10,
    ORV_MOUSE_WHEEL_DOWN    = ORV_MOUSE_BUTTON_5,
    ORV_MOUSE_BUTTON_6      = 0x20,
    ORV_MOUSE_WHEEL_LEFT    = ORV_MOUSE_BUTTON_6,
    ORV_MOUSE_BUTTON_7      = 0x40,
    ORV_MOUSE_WHEEL_RIGHT   = ORV_MOUSE_BUTTON_7,
    ORV_MOUSE_BUTTON_8      = 0x80

} orv_mouse_button_flag_t;

const orv_framebuffer_t* orv_acquire_framebuffer(orv_context_t* ctx);
void orv_release_framebuffer(orv_context_t* ctx);

const orv_cursor_t* orv_acquire_cursor(orv_context_t* ctx);
void orv_release_cursor(orv_context_t* ctx);

uint8_t orv_is_viewonly(const orv_context_t* ctx);
void orv_set_viewonly(orv_context_t* ctx, uint8_t viewOnly);

void orv_send_key_event(orv_context_t* ctx, int8_t isDown, uint32_t key);
void orv_send_key_event_unicode(orv_context_t* ctx, int8_t isDown, uint32_t unicode_key);
void orv_send_pointer_event(orv_context_t* ctx, int x, int y, uint8_t buttonMask);

/* Android specific key event handling */
void orv_android_on_key_down(orv_context_t* ctx, int keycode, int metaState, int unicodeChar);
void orv_android_on_key_up(orv_context_t* ctx, int keycode, int metaState, int unicodeChar);
void orv_android_on_key_multiple(orv_context_t* ctx, int keycode, int count, char* characters);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif

