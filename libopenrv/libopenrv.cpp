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

#include <libopenrv/libopenrv.h>

#include "orvvncclient.h"
#include "orv_context.h"
#include "eventqueue.h"
#include "keys.h"

#include <string.h>

int androidKeyEventToXKeyCode(int keycode, int metastate);

extern "C" {
/**
 * Set all variables in @p cfg to zero. This is equivalent to calling memset(cfg, 0,
 * sizeof(orv_config_t)).
 **/
void orv_config_zero(orv_config_t* cfg)
{
    memset(cfg, 0, sizeof(orv_config_t));
}

/**
 * Convenience function that copies the contents of config @p src to @p dst. This is equivalent to
 * performing memcpy(dst, src, sizeof(orv_config_t));
 **/
void orv_config_copy(orv_config_t* dst, const orv_config_t* src)
{
    memcpy(dst, src, sizeof(orv_config_t));
}

/**
 * Initialize @p cfg to default values.
 **/
void orv_config_default(orv_config_t* cfg)
{
    orv_config_zero(cfg);
    cfg->mLogCallback = orv_log_stdoutstderr;
    cfg->mEventCallback = orv_event_callback_polling;
}

orv_event_t* orv_event_init(orv_event_type_t type)
{
    // NOTE: we use malloc()/free() in this function, to simplify code:
    //       we can simply free() the mEventData void* pointer, no need to cast to the actual type
    //       (contrary to delete)
    orv_event_t* e = (orv_event_t*)malloc(sizeof(orv_event_t));
    memset(e, 0, sizeof(orv_event_t));
    e->mEventType = type;
    switch (type) {
        case ORV_EVENT_NONE:
        case ORV_EVENT_BELL:
        case ORV_EVENT_THREAD_STARTED:
        case ORV_EVENT_THREAD_ABOUT_TO_STOP:
        case ORV_EVENT_FRAMEBUFFER_UPDATE_REQUEST_FINISHED:
        case ORV_EVENT_CURSOR_UPDATED:
            break;
        case ORV_EVENT_DISCONNECTED:
            e->mEventData = malloc(sizeof(orv_disconnected_t));
            memset(e->mEventData, 0, sizeof(orv_disconnected_t));
            break;
        //case ORV_EVENT_FRAMEBUFFER:
        //    break;
        case ORV_EVENT_CONNECT_RESULT:
            e->mEventData = malloc(sizeof(orv_connect_result_t));
            memset(e->mEventData, 0, sizeof(orv_connect_result_t));
            break;
        case ORV_EVENT_CUT_TEXT:
            e->mEventData = malloc(sizeof(orv_cut_text_t));
            memset(e->mEventData, 0, sizeof(orv_cut_text_t));
            break;
        case ORV_EVENT_FRAMEBUFFER_UPDATED:
            e->mEventData = malloc(sizeof(orv_event_framebuffer_t));
            memset(e->mEventData, 0, sizeof(orv_event_framebuffer_t));
            break;
    }
    return e;
}

/**
 * Convenience function that initializes a ORV_EVENT_CONNECT_RESULT event with the required event
 * data.
 **/
orv_event_t* orv_event_connect_result_init(const char* hostName, uint16_t port, uint16_t width, uint16_t height, const char* desktopName, const orv_communication_pixel_format_t* pixelFormat, orv_auth_type_t authType, const orv_error_t* error)
{
    orv_event_t* e = orv_event_init(ORV_EVENT_CONNECT_RESULT);
    orv_connect_result_t* data = (orv_connect_result_t*)e->mEventData;
    data->mAuthenticationType = authType;
    strncpy(data->mHostName, hostName, ORV_MAX_HOSTNAME_LEN);
    data->mHostName[ORV_MAX_HOSTNAME_LEN] = '\0';
    data->mPort = port;
    if (pixelFormat) {
        orv_communication_pixel_format_copy(&data->mCommunicationPixelFormat, pixelFormat);
    }
    else {
        orv_communication_pixel_format_reset(&data->mCommunicationPixelFormat);
    }
    free(data->mDesktopName);
    if (error) {
        orv_error_copy(&data->mError, error);
        data->mFramebufferWidth = 0;
        data->mFramebufferHeight = 0;
        data->mDesktopName = (char*)malloc(1);
        data->mDesktopName[0] = '\0';
    }
    else {
        orv_error_reset(&data->mError);
        data->mFramebufferWidth = width;
        data->mFramebufferHeight = height;
        if (desktopName) {
            size_t s = strlen(desktopName);
            s = std::min(s, (size_t)ORV_MAX_DESKTOP_NAME_LENGTH);
            data->mDesktopName = (char*)malloc(s + 1);
            memcpy(data->mDesktopName, desktopName, s);
            data->mDesktopName[s] = '\0';
        }
        else {
            data->mDesktopName = (char*)malloc(1);
            data->mDesktopName[0] = '\0';
        }
    }
    return e;
}

/**
 * Convenience function that initializes a ORV_EVENT_DISCONNECTED event with the required event
 * data.
 *
 * @param gracefulExit Whether this is a graceful exit (requested by the user) or actually an error.
 *        This is a convenience parameter: If @p gracefulExit is 0 but @p error indicates success,
 *        the error in the event is set to a generic error.
 **/
orv_event_t* orv_event_disconnected_init(const char* hostName, uint16_t port, uint8_t gracefulExit, const orv_error_t* error)
{
    orv_event_t* e = orv_event_init(ORV_EVENT_DISCONNECTED);
    orv_disconnected_t* data = (orv_disconnected_t*)e->mEventData;
    strncpy(data->mHostName, hostName, ORV_MAX_HOSTNAME_LEN);
    data->mHostName[ORV_MAX_HOSTNAME_LEN] = '\0';
    data->mPort = port;
    if (error) {
        orv_error_copy(&data->mError, error);
    }
    else {
        orv_error_reset(&data->mError);
    }
    if (!gracefulExit && !data->mError.mHasError) {
        orv_error_set(&data->mError, ORV_ERR_GENERIC, 9000, "Disconnected due to unknown error.");
    }
    return e;
}

orv_event_t* orv_event_framebuffer_init(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    orv_event_t* e = orv_event_init(ORV_EVENT_FRAMEBUFFER_UPDATED);
    orv_event_framebuffer_t* data = (orv_event_framebuffer_t*)e->mEventData; /* FIXME: rename orv_connect_result_t, orv_disconnected_t etc. to orv_event_* */
    data->mX = x;
    data->mY = y;
    data->mWidth = w;
    data->mHeight = h;
    return e;
}

/**
 * Convenience function that creates and initializes a ORV_EVENT_CUT_TEXT event for a "server cut
 * text" message, i.e. data from the server for the clipboard.
 *
 * The @p text must be latin-1 encoding, VNC currently does not provide support for non-latin-1
 * data.
 **/
orv_event_t* orv_event_cut_text_init(const char* text)
{
    orv_event_t* e = orv_event_init(ORV_EVENT_CUT_TEXT);
    orv_cut_text_t* data = (orv_cut_text_t*)e->mEventData;
    size_t len = strlen(text);
    len = std::min(len, (size_t)ORV_MAX_VNC_SERVER_CUT_TEXT_SIZE);
    data->mText = (char*)malloc(len + 1);
    strncpy(data->mText, text, len);
    data->mText[len] = '\0';
    return e;
}

/**
 * Free the specified event. This in particular also frees the event data (if any).
 **/
void orv_event_destroy(orv_event_t* event)
{
    if (!event) {
        return;
    }
    if (event->mEventData) {
        switch (event->mEventType) {
            case ORV_EVENT_CONNECT_RESULT:
            {
                orv_connect_result_t* data = (orv_connect_result_t*)event->mEventData;
                free(data->mDesktopName);
                break;
            }
            case ORV_EVENT_CUT_TEXT:
            {
                orv_cut_text_t* data = (orv_cut_text_t*)event->mEventData;
                free(data->mText);
                break;
            }
            default:
                break;
        }
        free(event->mEventData);
    }
    free(event);
}

/**
 * Convenience/debugging function that dumps @p event to the log callback in @p ctx.
 **/
void orv_event_print_to_log(const struct orv_context_t* ctx, const orv_event_t* event)
{
    switch (event->mEventType) {
        case ORV_EVENT_NONE:
            ORV_DEBUG(ctx, "ORV_EVENT_NONE");
            break;
        case ORV_EVENT_CONNECT_RESULT:
        {
            orv_connect_result_t* data = (orv_connect_result_t*)event->mEventData;
            ORV_DEBUG(ctx, "ORV_EVENT_CONNECT_RESULT to host %s:%d: %s", data->mHostName, (int)data->mPort, data->mError.mHasError ? "ERROR" : "SUCCESS");
            if (data->mError.mHasError) {
                orv_error_print_to_log(ctx, &data->mError);
            }
            break;
        }
        case ORV_EVENT_DISCONNECTED:
        {
            orv_disconnected_t* data = (orv_disconnected_t*)event->mEventData;
            ORV_DEBUG(ctx, "ORV_EVENT_DISCONNECTED from host %s:%d, error code: %d.%d (%s)", data->mHostName, (int)data->mPort, (int)data->mError.mErrorCode, (int)data->mError.mSubErrorCode, data->mError.mHasError ? data->mError.mErrorMessage : "no error");
            break;
        }
        case ORV_EVENT_BELL:
            ORV_DEBUG(ctx, "ORV_EVENT_BELL");
            break;
        case ORV_EVENT_CUT_TEXT:
        {
            orv_cut_text_t* data = (orv_cut_text_t*)event->mEventData;
            size_t len = strlen(data->mText);
            ORV_DEBUG(ctx, "ORV_EVENT_CUT_TEXT with %u bytes text: %s", (unsigned int)len, data->mText);
            break;
        }
        case ORV_EVENT_FRAMEBUFFER_UPDATED:
        {
            orv_event_framebuffer_t* data = (orv_event_framebuffer_t*)event->mEventData;
            ORV_DEBUG(ctx, "ORV_EVENT_FRAMEBUFFER_UPDATED at x=%d y=%d size=%dx%d", (int)data->mX, (int)data->mY, (int)data->mWidth, (int)data->mHeight);
            break;
        }
        case ORV_EVENT_THREAD_STARTED:
            ORV_DEBUG(ctx, "ORV_EVENT_THREAD_STARTED, thread name: %s", (const char*)event->mEventData);
            break;
        case ORV_EVENT_THREAD_ABOUT_TO_STOP:
            ORV_DEBUG(ctx, "ORV_EVENT_THREAD_ABOUT_TO_STOP, thread name: %s", (const char*)event->mEventData);
            break;
        case ORV_EVENT_FRAMEBUFFER_UPDATE_REQUEST_FINISHED:
        {
            ORV_DEBUG(ctx, "ORV_EVENT_FRAMEBUFFER_UPDATE_REQUEST_FINISHED");
            break;
        }
        case ORV_EVENT_CURSOR_UPDATED:
        {
            ORV_DEBUG(ctx, "ORV_EVENT_CURSOR_UPDATED");
            break;
        }
    }
}

/**
 * @return A new context object that can be used to initiate a connection to a server, or NULL if
 *         intializing the context failed for some reason (e.g. invalid config in @p cfg or no more
 *         file descriptors available on this system).
 **/
orv_context_t* orv_init(const orv_config_t* cfg)
{
    if (!cfg->mEventCallback) {
        // TODO: send an error event to ctx->mConfig.mEventCallback ?
        //       however this would *require* the callback to be thread-safe, because we are not yet
        //       in the connection thread at this point
        return nullptr;
    }

    orv_context_t* ctx = new orv_context_t();
    orv_config_copy(&ctx->mConfig, cfg);
    for (int i = 0; i < ORV_USER_DATA_COUNT; i++) {
        orv_set_user_data(ctx, (orv_user_data_t)i, cfg->mUserData[i]);
    }
    orv_error_t error;
    ORV_DEBUG(ctx, "orv_init()");
    if (ctx->mConfig.mEventCallback == orv_event_callback_polling) {
        ctx->mEventQueue = new openrv::EventQueue(ctx);
    }
    ctx->mClient = new openrv::vnc::OrvVncClient(ctx, &error);
    if (error.mHasError) {
        ORV_ERROR(ctx, "Failed to construct the internal ORV client object, error:");
        orv_error_print_to_log(ctx, &error);
        // TODO: send an error event to ctx->mConfig.mEventCallback ?
        //       however this would *require* the callback to be thread-safe, because we are not yet
        //       in the connection thread at this point
        //       (and may never have started such a thread)
        delete ctx;
        return nullptr;
    }
    ORV_DEBUG(ctx, "New context initialized: %p", ctx);
    return ctx;
}

void orv_destroy(orv_context_t* ctx)
{
    ORV_DEBUG(ctx, "Destroying context %p", ctx);
    if (ctx) {
        delete ctx->mClient;
        delete ctx->mEventQueue;
        delete ctx;
    }
}

/**
 * @return A string representation of @p qualityProfile.
 *         This string can be used to serialize the @p qualityProfile into some settings, as it is
 *         mean to never change, even if the actual enum values change.
 **/
const char* orv_get_communication_quality_profile_string(orv_communication_quality_profile_t qualityProfile)
{
    switch (qualityProfile) {
        case ORV_COMM_QUALITY_PROFILE_LOW:
            return "QualityProfileLow";
        case ORV_COMM_QUALITY_PROFILE_MEDIUM:
            return "QualityProfileMedium";
        case ORV_COMM_QUALITY_PROFILE_BEST:
            return "QualityProfileBest";
        case ORV_COMM_QUALITY_PROFILE_SERVER:
            return "QualityProfileServer";
        case ORV_COMM_QUALITY_PROFILE_CUSTOM:
            return "QualityProfileCustom";
    }
    return "QualityProfileUnknown";
}

/**
 * @return The @ref orv_communication_quality_profile_t value for @p string, as returned by @ref
 *         orv_get_communication_quality_profile_string().
 *         If @p string is not known, @p fallback is returned instead.
 **/
orv_communication_quality_profile_t orv_get_communication_quality_profile_from_string(const char* string, orv_communication_quality_profile_t fallback)
{
    if (strcmp(string, "QualityProfileLow") == 0) {
        return ORV_COMM_QUALITY_PROFILE_LOW;
    }
    if (strcmp(string, "QualityProfileMedium") == 0) {
        return ORV_COMM_QUALITY_PROFILE_MEDIUM;
    }
    if (strcmp(string, "QualityProfileBest") == 0) {
        return ORV_COMM_QUALITY_PROFILE_BEST;
    }
    if (strcmp(string, "QualityProfileServer") == 0) {
        return ORV_COMM_QUALITY_PROFILE_SERVER;
    }
    if (strcmp(string, "QualityProfileCustom") == 0) {
        return ORV_COMM_QUALITY_PROFILE_CUSTOM;
    }
    return fallback;
}

/**
 * Reset @p format to default values provided by this library.
 **/
void orv_communication_pixel_format_reset(orv_communication_pixel_format_t* format)
{
    memset(format, 0, sizeof(orv_communication_pixel_format_t));
    format->mBigEndian = 0;
    format->mTrueColor = 1;
    format->mBitsPerPixel = 32;
    format->mDepth = 24;
    format->mColorMax[0] = 255;
    format->mColorMax[1] = 255;
    format->mColorMax[2] = 255;
    format->mColorShift[0] = 16;
    format->mColorShift[1] = 8;
    format->mColorShift[2] = 0;
}

/**
 * Copy @p src to @p dst.
 **/
void orv_communication_pixel_format_copy(orv_communication_pixel_format_t* dst, const orv_communication_pixel_format_t* src)
{
    if (!dst) {
        return;
    }
    if (!src) {
        orv_communication_pixel_format_reset(dst);
        return;
    }
    memcpy(dst, src, sizeof(orv_communication_pixel_format_t));
}

void orv_connection_info_reset(orv_connection_info_t* info)
{
    if (info) {
        memset(info, 0, sizeof(orv_connection_info_t));
    }
}

void orv_connection_info_copy(orv_connection_info_t* dst, const orv_connection_info_t* src)
{
    if (!dst) {
        return;
    }
    if (!src) {
        orv_connection_info_reset(dst);
        return;
    }
    memcpy(dst, src, sizeof(orv_connection_info_t));
}

void orv_connection_info_print_to_log(const struct orv_context_t* ctx, const orv_connection_info_t* info)
{
    ORV_DEBUG(ctx, "Connection info:");
    if (!info) {
        ORV_DEBUG(ctx, "(NULL):");
        return;
    }
    if (info->mConnected != 1) {
        ORV_DEBUG(ctx, "  Not connected.");
        return;
    }
    ORV_DEBUG(ctx, "  Connected to '%s' on port %d, desktop name is '%s'", info->mHostName, (int)info->mPort, info->mDesktopName);
    ORV_DEBUG(ctx, "  Framebuffer size: %dx%d", info->mFramebufferWidth, info->mFramebufferHeight);
    const orv_communication_pixel_format_t* p = &info->mCommunicationPixelFormat;
    ORV_DEBUG(ctx, "  Pixel format of communication: %d BitsPerPixel, %d bits depth, TrueColor: %s, r/g/b max: %d/%d/%d, r/g/b shift: %d/%d/%d, BigEndian: %s", p->mBitsPerPixel, p->mDepth, p->mTrueColor ? "true" : "false", (int)p->mColorMax[0], (int)p->mColorMax[1], (int)p->mColorMax[2], (int)p->mColorShift[0], (int)p->mColorShift[1], (int)p->mColorShift[2], p->mBigEndian ? "true" : "false");
    ORV_DEBUG(ctx, "  Received bytes: %u, sent bytes: %u, total: %u", info->mReceivedBytes, info->mSentBytes, (info->mReceivedBytes + info->mSentBytes));
}

void orv_vnc_server_capabilities_reset(orv_vnc_server_capabilities_t* capabilities)
{
    if (capabilities) {
        memset(capabilities, 0, sizeof(orv_vnc_server_capabilities_t));
        capabilities->mSupportedEncodingCapabilitiesPartial = 1;
    }
}

void orv_vnc_server_capabilities_copy(orv_vnc_server_capabilities_t* dst, const orv_vnc_server_capabilities_t* src)
{
    if (!dst) {
        return;
    }
    if (!src) {
        orv_vnc_server_capabilities_reset(dst);
        return;
    }
    memcpy(dst, src, sizeof(orv_vnc_server_capabilities_t));
}

void orv_vnc_server_capabilities_print_to_log(const struct orv_context_t* ctx, const orv_vnc_server_capabilities_t* capabilities)
{
    using namespace openrv;
    using namespace openrv::vnc;
    ORV_DEBUG(ctx, "Server Capabilities info:");
    if (!capabilities) {
        ORV_DEBUG(ctx, "(NULL):");
        return;
    }
    ORV_DEBUG(ctx, "  %d supported security types:", (int)capabilities->mSupportedSecurityTypesCount);
    for (int i = 0; i < (int)capabilities->mSupportedSecurityTypesCount; i++) {
        uint8_t s = capabilities->mSupportedSecurityTypes[i];
        ORV_DEBUG(ctx, "    %3d (%s)", s, OrvVncClient::getSecurityTypeString((SecurityType)s));
    }
    ORV_DEBUG(ctx, "  %d supported encoding types:", (int)capabilities->mSupportedEncodingCapabilitiesCount);
    for (int i = 0; i < (int)capabilities->mSupportedEncodingCapabilitiesCount; i++) {
        const orv_vnc_tight_capability_t* c = &capabilities->mSupportedEncodingCapabilities[i];
        ORV_DEBUG(ctx, "    %4d (vendor: %s, signature: %s) - %s", c->mCode, c->mVendor, c->mSignature, OrvVncClient::getEncodingTypeString((openrv::vnc::EncodingType)c->mCode));
    }
    if (capabilities->mSupportedEncodingCapabilitiesPartial) {
        ORV_DEBUG(ctx, "  (encodings list may be partial, no full capabilities list from server available)");
    }
    else {
        ORV_DEBUG(ctx, "  (encodings list is complete)");
    }
    if (capabilities->mSupportedServerMessageCapabilitiesCount > 0 || capabilities->mSupportedClientMessageCapabilitiesCount > 0) {
        ORV_DEBUG(ctx, "  %d supported server message capabilities:", (int)capabilities->mSupportedServerMessageCapabilitiesCount);
        for (int i = 0; i < (int)capabilities->mSupportedServerMessageCapabilitiesCount; i++) {
            const orv_vnc_tight_capability_t* c = &capabilities->mSupportedServerMessageCapabilities[i];
            ORV_DEBUG(ctx, "    %4d (vendor: %s, signature: %s)", c->mCode, c->mVendor, c->mSignature);
        }
        ORV_DEBUG(ctx, "  %d supported client message capabilities:", (int)capabilities->mSupportedClientMessageCapabilitiesCount);
        for (int i = 0; i < (int)capabilities->mSupportedClientMessageCapabilitiesCount; i++) {
            const orv_vnc_tight_capability_t* c = &capabilities->mSupportedClientMessageCapabilities[i];
            ORV_DEBUG(ctx, "    %4d (vendor: %s, signature: %s)", c->mCode, c->mVendor, c->mSignature);
        }
    }
}


void orv_get_vnc_connection_info(const struct orv_context_t* ctx, orv_connection_info_t* info, orv_vnc_server_capabilities_t* capabilities)
{
    ctx->mClient->getInfo(info, capabilities);
}

/**
 * Reset the @p options to default values provided by this library.
 **/
void orv_connect_options_default(orv_connect_options_t* options)
{
    memset(options, 0, sizeof(orv_connect_options_t));
    options->mViewOnly = 0;
    orv_communication_pixel_format_reset(&options->mCommunicationPixelFormat);
    // TODO: which one to use as default? probably use an adaptive type by default
    options->mCommunicationQualityProfile = ORV_COMM_QUALITY_PROFILE_SERVER;
}

/**
 * Copy the options provided by @p src to @p dst.
 **/
void orv_connect_options_copy(orv_connect_options_t* dst, const orv_connect_options_t* src)
{
    if (!dst) {
        return;
    }
    if (!src) {
        orv_connect_options_default(dst);
        return;
    }
    memcpy(dst, src, sizeof(orv_connect_options_t));
}

/**
  * Set username and/or password for the current or future connection.
  **/
int orv_set_credentials(orv_context_t* ctx, const char* user, const char* password)
{
    if (!ctx->mClient->setCredentials(user, password)) {
        return 1;
    }
    return 0;
}

/**
 * @param options If non-NULL, this parameter holds additional options for the connection.
 *        If NULL, this function behaves as-if default values were used, see @ref
 *        orv_connect_options_reset().
 * @param error Optional output parameter that will provide additional information if this function
 *        returns a non-zero value.
 *        Note that this value is only used if connecting fails @em immediately. If it fails
 *        asynchronously (which normally is the case), this parameter will not receive the error.
 *
 * Initiate a connection to the specified @p host on the specified @p port.
 *
 * If the connection could be initiated, this function returns 0, otherwise a non-zero value is
 * returned and @p error is set accordingly (if non-NULL).
 *
 * If this function returns 0, the connection has merely been @em initiated, the connection did not
 * yet succeed.
 *
 * @return 0 if the connection process was initiated, a non-zero value if the connection failed
 *         immediately, e.g. because another connection was still active for @p ctx or if @p host or
 *         @p port did not contain valid data.
 **/
// TODO: docs: document what happens if connection has been established or if it has been rejected
// (what callback function is called with the event).
int orv_connect(orv_context_t* ctx, const char* host, uint16_t port, const orv_connect_options_t* options, orv_error_t* error)
{
    if (error) {
        orv_error_reset(error);
    }
    orv_connect_options_t defaultOptions;
    if (!options) {
        orv_connect_options_default(&defaultOptions);
        options = &defaultOptions;
    }
    if (!ctx->mClient->connectToHost(host, port, options, error)) {
        return 1;
    }
    return 0;
}

/**
 * Initiate a disconnect. This function disconnects the @p ctx from the remote host
 * (if connected) and sends a ORV_EVENT_DISCONNECTED event (even if no connection was made yet).
 *
 * This function will
 * @li disconnect from the remote host, if connected.
 * @li abort a connection attempt, if a connection was initiated, but not yet finished.
 *     Note that the ORV_EVENT_CONNECT_RESULT event (indicating user interruption) will be sent
 *     prior to the ORV_EVENT_DISCONNECTED event.
 * @li do nothing (except for sending the ORV_EVENT_DISCONNECTED event that is always sent) if no
 *     connection was made yet.
 *
 * Note that multiple calls to this function may get compressed into a single
 * ORV_EVENT_DISCONNECTED event: There is no guarantee that every call generates exactly one event,
 * but there is a guarantee that there will be a ORV_EVENT_DISCONNECTED event after this call.
 **/
void orv_disconnect(orv_context_t* ctx)
{
    ctx->mClient->disconnect();
}

/**
 * @return Whether the context is currently connected or connecting (boolean).
 *         1 if ctx is connected or connecting, otherwise 0.
 **/
int orv_is_connected(orv_context_t* ctx)
{
    if (ctx->mClient->isConnected()) {
        return 1;
    }
    return 0;
}

/**
 * Store @p userData at @p index in the @p ctx. The pointer is simply copied, no memory is
 * allocated.
 * The value can be retrieved using @ref orv_get_user_data().
 *
 * The user-data is meant for users of this library to store custom data. The library does not use
 * or modify the data in any way.
 **/
void orv_set_user_data(orv_context_t* ctx, orv_user_data_t index, void* userData)
{
    if (!ctx) {
        return;
    }
    if ((int)index < 0 || index >= ORV_USER_DATA_COUNT) {
        return;
    }
    ctx->mUserData[index].mPointerData = userData;
}

/**
 * Convenience function to store integer data as user data instead of a pointer.
 *
 * See @ref orv_set_user_data().
 **/
void orv_set_user_data_int(orv_context_t* ctx, orv_user_data_t index, int userData)
{
    if (!ctx) {
        return;
    }
    if ((int)index < 0 || index >= ORV_USER_DATA_COUNT) {
        return;
    }
    ctx->mUserData[index].mIntData = userData;
}

/**
 * Retrieved previously stored user data. See also @ref orv_set_user_data().
 *
 * If no data was stored, this function always returns NULL.
 **/
void* orv_get_user_data(orv_context_t* ctx, orv_user_data_t index)
{
    if (!ctx) {
        return nullptr;
    }
    if ((int)index < 0 || index >= ORV_USER_DATA_COUNT) {
        return nullptr;
    }
    return ctx->mUserData[index].mPointerData;
}

/**
 * Convenience function to retrieve integer data as user data instead of a pointer.
 *
 * See @ref orv_get_user_data().
 **/
int orv_get_user_data_int(orv_context_t* ctx, orv_user_data_t index)
{
    if (!ctx) {
        return 0;
    }
    if ((int)index < 0 || index >= ORV_USER_DATA_COUNT) {
        return 0;
    }
    return ctx->mUserData[index].mIntData;
}

/**
 * Convenience function that is equivalent to @ref orv_request_framebuffer_update() with the full
 * framebuffer size.
 **/
void orv_request_framebuffer_update_full(orv_context_t* ctx)
{
    const bool incremental = true;
    ctx->mClient->sendFramebufferUpdateRequest(incremental);
}

/**
 * Send a request to the server asking to get notified whenever the framebuffer in the specified
 * rectangle changes.
 *
 * The server will will respond to this message with a framebuffer update message, however note that
 * this message can be sent indefinitely in the future (the server decides when to send another
 * update). This library provides these updates in zero or more ORV_EVENT_FRAMEBUFFER_UPDATED events
 * (zero such events are generated if the server sent pseudo-encoding data only) and exactly one
 * ORV_EVENT_FRAMEBUFFER_UPDATE_REQUEST_FINISHED event that marks the end of this request.
 **/
void orv_request_framebuffer_update(orv_context_t* ctx, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    const bool incremental = true;
    ctx->mClient->sendFramebufferUpdateRequest(incremental, x, y, w, h);
}

/**
 * Implementation of an event callback function that queues all events into an internal event queue
 * and provides access to it using the @ref orv_poll_event() function.
 *
 * This function should never be called directly, it is meant to be set as callback function prior
 * to creating the context.
 **/
void orv_event_callback_polling(orv_context_t* ctx, orv_event_t* event)
{
    if (!ctx->mEventQueue) {
        ORV_ERROR(ctx, "NULL event queue, invalid context used in callback");
        orv_event_destroy(event);
        return;
    }
    switch (event->mEventType) {
        case ORV_EVENT_THREAD_STARTED:
        case ORV_EVENT_THREAD_ABOUT_TO_STOP:
            /* these events are ignored for polling based event loops */
            orv_event_destroy(event);
            return;
        default:
            break;
    }
    //ORV_DEBUG(ctx, "Queue event: %d (%s)", (int)event->mEventType, OrVncvClient::getEventTypeString(event->mEventType));
    ctx->mEventQueue->queue(event);
}

/**
 * If the @ref orv_event_callback_polling() function is used as event callback in @p ctx, this
 * function can be used to poll for events from the library.
 *
 * @return The next event from the library for @p ctx, if one is available, otherwise NULL.
 *         This function always returns NULL, if @ref orv_context_t::mEventCallback of @p ctx is not
 *         @ref orv_event_callback_polling.
 **/
orv_event_t* orv_poll_event(orv_context_t* ctx)
{
    if (!ctx) {
        return nullptr;
    }
    if (!ctx->mEventQueue) {
        return nullptr;
    }
    return ctx->mEventQueue->dequeue();
}

/**
 * Convenience and debugging function to map a message type value of the RFB protocol to a human
 * readable string.
 *
 * @return A human readable string describing the message type @p clientMessageType, which must be a
 *         "client to server" message. if @p clientMessageType is not known, the returned string
 *         will indicate so (and still be a valid string).
 *         The returned pointer points to static memory and must not be written to. It remains valid
 *         even after destruction of all valid OpenRV contexts.
 **/
const char* orv_get_client_message_type_string(uint8_t clientMessageType)
{
    return openrv::vnc::OrvVncClient::getClientMessageTypeString((openrv::vnc::ClientMessage)clientMessageType);
}

/**
 * Convenience and debugging function to map a message type value of the RFB protocol to a human
 * readable string.
 *
 * @return A human readable string describing the message type @p serverMessageType, which must be a
 *         "server to client" message. If @p serverMessageType is not known, the returned string
 *         will indicate so (and still be a valid string).
 *         The returned pointer points to static memory and must not be written to. It remains valid
 *         even after destruction of all valid OpenRV contexts.
 **/
const char* orv_get_vnc_server_message_type_string(uint8_t serverMessageType)
{
    return openrv::vnc::OrvVncClient::getServerMessageTypeString((openrv::vnc::ServerMessage)serverMessageType);
}
const char* orv_get_vnc_security_type_string(uint8_t securityType)
{
    return openrv::vnc::OrvVncClient::getSecurityTypeString((openrv::vnc::SecurityType)securityType);
}
const char* orv_get_vnc_encoding_type_string(int32_t encodingType)
{
    return openrv::vnc::OrvVncClient::getEncodingTypeString((openrv::vnc::EncodingType)encodingType);
}

/**
 * Obtain the framebuffer pointer and lock it for reading. The caller @em must release the
 * framebuffer using @ref orv_release_framebuffer() after use, otherwise the library is deadlocked.
 **/
const orv_framebuffer_t* orv_acquire_framebuffer(orv_context_t* ctx)
{
    if (!ctx) {
        return nullptr;
    }
    return ctx->mClient->acquireFramebuffer();
}

/**
 * Release a previously locked framebuffer pointer.
 * After this call, the pointer returned by @ref orv_acquire_framebuffer() must @em not be used
 * anymore for anything.
 *
 * There must be exactly one release call per acquire call. If a call is missing, the application
 * may deadlock. If a framebuffer is released that was never acquired before, the application may
 * crash.
 **/
void orv_release_framebuffer(orv_context_t* ctx)
{
    if (!ctx) {
        return;
    }
    ctx->mClient->releaseFramebuffer();
}

/**
 * Obtain the cursor data pointer and lock it for reading. This function is similar to @ref
 * orv_acquire_framebuffer() but for cursor data. The library notifies when new cursor data is
 * available via a @ref ORV_EVENT_CURSOR_DATA event.
 *
 * The pointer returned by this function only contains valid data once the first @ref
 * ORV_EVENT_CURSOR_UPDATED event has been received. The @ref orv_event_t::mIsValid flag indicates
 * whether this cursor provides valid data - if set to 0, the returned pointer should be ignored
 * (and released again).
 *
 * The returned pointer must be released using @ref orv_release_cursor() after use.
 *
 * NOTE: The library may use the same mutex to lock cursor and framebuffer data, so it is @em not
 *       possible to acquire both framebuffer and cursor data simultaneously.
 **/
const orv_cursor_t* orv_acquire_cursor(orv_context_t* ctx)
{
    if (!ctx) {
        return nullptr;
    }
    return ctx->mClient->acquireCursor();
}

/**
 * Similar to @ref orv_release_framebuffer(), but for cursor data, see @ref orv_acquire_cursor().
 **/
void orv_release_cursor(orv_context_t* ctx)
{
    if (!ctx) {
        return;
    }
    ctx->mClient->releaseCursor();
}

/**
 * @return 1 if capabilities @p t1 and @p t2 are equal, otherwise 0.
 *         NOTE: This requires that BOTH capabilities are of the same type (encodings, server
 *         messages, client messages, ...), otherwise the capabilities are not comparable with each
 *         other.
 **/
uint8_t orv_are_capabilities_equal(const orv_vnc_tight_capability_t* t1, const orv_vnc_tight_capability_t* t2)
{
    if (t1->mCode == t2->mCode &&
            memcmp(t1->mVendor, t2->mVendor, 4) == 0 &&
            memcmp(t1->mSignature, t2->mSignature, 8) == 0) {
        return 1;
    }
    return 0;
}

/**
 * @return A boolean indicating whether the @p ctx is used in "viewonly" mode. If viewonly is
 *         active, this function returns 1 and this library will not send any pointer or key events
 *         to the remote host. Otherwise this function returns 0.
 **/
uint8_t orv_is_viewonly(const orv_context_t* ctx)
{
    if (ctx) {
        if (ctx->mClient->isViewOnly()) {
            return 1;
        }
        return 0;
    }
    return 1;
}

/**
 * Set the viewonly flag, see @ref orv_is_viewonly() for details.
 *
 * @param viewOnly Whether viewonly is enabled. 0 to disable viewOnly mode, nonzero value to enable
 *        viewOnly mode.
 **/
void orv_set_viewonly(orv_context_t* ctx, uint8_t viewOnly)
{
    if (ctx) {
        ctx->mClient->setViewOnly(viewOnly != 0);
    }
}

// TODO: provide specialized frontend functions for various platforms, e.g.
//   orv_send_key_event_android(), orv_send_key_event_x11(), orv_send_key_event_win32()
// -> they should provide the proper mapping from platform parameters to RFB keycodes.
/**
 * @param isDown Boolean value, 1 if the @p key is pressed, 0 if the @p is released.
 * @param key Keycode, as specified by the RFB protocol.
 *
 * Send a key event to the server.
 *
 * The @p key is a keyCode, as specified by the RFB protocol.
 *
 * If @ref orv_is_viewonly() returns 1, this function does nothing.
 **/
void orv_send_key_event(orv_context_t* ctx, int8_t isDown, uint32_t key)
{
    if (!ctx) {
        return;
    }
    if (orv_is_viewonly(ctx)) {
        return;
    }
    ctx->mClient->sendKeyEvent((isDown != 0), key);
}

void orv_send_key_event_unicode(orv_context_t* ctx, int8_t isDown, uint32_t unicode_key)
{
    if (!ctx) {
        return;
    }
    if (orv_is_viewonly(ctx)) {
        return;
    }
    int xkeyCode = unicodeToXKeyCode(ctx, unicode_key);
    if (xkeyCode == -1) {
        return;
    }
    ctx->mClient->sendKeyEvent((isDown != 0), xkeyCode);
}


/**
 * Send a pointer event with the specified coordinates to the server.
 *
 * The @p buttonMask should hold all currently pressed mouse button, see @ref
 * orv_mouse_button_flag_t.
 *
 * Mouse wheel can be sent by sending a pointer event with the corresponding wheel "button" (e.g.
 * @ref ORV_MOUSE_WHEEL_UP) set to "pressed", followed by an immediate event with that "button"
 * released again.
 *
 * The @p x and @p y coordinates are automatically clipped to the valid framebuffer rectangle.
 *
 * If @ref orv_is_viewonly() returns 1, this function does nothing.
 **/
void orv_send_pointer_event(orv_context_t* ctx, int x, int y, uint8_t buttonMask)
{
    if (!ctx) {
        return;
    }
    if (orv_is_viewonly(ctx)) {
        return;
    }
    ctx->mClient->sendPointerEvent(x, y, buttonMask);
}

void orv_android_on_key_down(orv_context_t* ctx, int keycode, int metaState, int unicodeChar)
{
    if (!ctx) {
        return;
    }
    ORV_DEBUG(ctx, "orv_android_on_key_down(keycode=%d, metaState=%d)", keycode, metaState);
    int xkey = -1;
    if (unicodeChar != 0)
    {
        xkey = unicodeToXKeyCode(ctx, unicodeChar);
    }

    if (xkey == -1)
    {
         xkey = androidKeyEventToXKeyCode(keycode, metaState);
    }
    //ORV_DEBUG(ctx, "Droid2X: %d -> %d", keycode, xkey);
    if (xkey != -1)
    {
        orv_send_key_event(ctx, 1, xkey);
    }
}
void orv_android_on_key_up(orv_context_t* ctx, int keycode, int metaState, int unicodeChar)
{
    if (!ctx) {
        return;
    }
    ORV_DEBUG(ctx, "orv_android_on_key_up(keycode=%d, metaState=%d)", keycode, metaState);
    int xkey = -1;
    if (unicodeChar != 0)
    {
        xkey = unicodeToXKeyCode(ctx, unicodeChar);
    }
    if (xkey == -1)
    {
        xkey =  androidKeyEventToXKeyCode(keycode, metaState);
    }
    if (xkey != -1)
    {
        orv_send_key_event(ctx, 0, xkey);
    }
}
void orv_android_on_key_multiple(orv_context_t* ctx, int keycode, int count, char* characters)
{
    if (!ctx) {
        return;
    }
    ORV_DEBUG(ctx, "orv_android_on_key_multiple(keycode=%d, count=%d, chars=%s)", keycode, count, characters);
    int xkeys[256];
    int xkeyCount = utf8CharsToXKeys(ctx, characters, xkeys, sizeof(xkeys) / sizeof(int));
    for (int i = 0; i < xkeyCount; i++)
    {
        ORV_DEBUG(ctx, "sending xkey event %d", xkeys[i]);
        orv_send_key_event(ctx, 1, xkeys[i]);
        orv_send_key_event(ctx, 0, xkeys[i]);
    }


}
} // extern "C"
