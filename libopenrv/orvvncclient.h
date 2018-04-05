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

#ifndef OPENRV_ORVCLIENT_H
#define OPENRV_ORVCLIENT_H

#include <libopenrv/libopenrv.h>
#include "rfbtypes.h"

#include <stdint.h>
#include <mutex>
#include <thread>

struct orv_context_t;
struct orv_error_t;

namespace openrv {

class ThreadNotifierWriter;

namespace vnc {

class ConnectionThread;
struct OrvVncClientSharedData;


class OrvVncClient
{
public:
    OrvVncClient(orv_context_t* ctx, orv_error_t* error);
    virtual ~OrvVncClient();

    void setSharedAccess(bool shared);
    bool isSharedAccess() const;
    bool connectToHost(const char* hostName, uint16_t port, const char* password, const struct orv_connect_options_t* options, orv_error_t* error = nullptr);
    void disconnect();
    bool isConnected() const;
    void sendFramebufferUpdateRequest(bool incremental, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void sendFramebufferUpdateRequest(bool incremental);
    void sendKeyEvent(bool down, uint32_t key);
    void sendPointerEvent(int x, int y, uint8_t buttonMask);
    void getInfo(orv_connection_info_t* info, orv_vnc_server_capabilities_t* capabilities);
    bool isViewOnly() const;
    void setViewOnly(bool viewOnly);

    const orv_framebuffer_t* acquireFramebuffer();
    void releaseFramebuffer();
    const orv_cursor_t* acquireCursor();
    void releaseCursor();

    static void makePixelFormat(orv_communication_pixel_format_t* format, int bitsPerPixel);
    static void makePixelFormat(orv_communication_pixel_format_t* format, const orv_context_t* context, orv_communication_quality_profile_t profile, const orv_communication_pixel_format_t* serverFormat, const orv_communication_pixel_format_t* customFormat);
    static bool isPixelFormatValidForReceive(const orv_communication_pixel_format_t& format);
    static bool isPixelFormatValidForSend(const orv_communication_pixel_format_t& format);
    static const char* getClientMessageTypeString(vnc::ClientMessage clientMessageType);
    static const char* getServerMessageTypeString(vnc::ServerMessage serverMessageType);
    static const char* getSecurityTypeString(vnc::SecurityType securityType);
    static const char* getEncodingTypeString(vnc::EncodingType encodingType);
    static const char* getEventTypeString(orv_event_type_t eventType);

protected:
    void wakeThread();
    void sendSetPixelFormat(const orv_communication_pixel_format_t& pixelFormat); // NOTE: must be internal, as it must not be sent when a FramebufferUpdateRequest is still pending

private:
    orv_context_t* mContext = nullptr;
    std::thread* mThread = nullptr;
    ThreadNotifierWriter* mPipeWriter = nullptr;
    static const uint16_t mDefaultPort = 5900;
    char mHostName[ORV_MAX_HOSTNAME_LEN + 1] = {};
    uint16_t mPort = mDefaultPort;
    bool mSharedAccess = false;
    bool mViewOnly = false;

    /**
     * Data shared between this object and the internal @ref ConnectionThread.
     *
     * All data in this object must be protected by a mutex.
     **/
    OrvVncClientSharedData* mCommunicationData = nullptr;
};
bool operator==(const orv_communication_pixel_format_t& p1, const orv_communication_pixel_format_t& p2);
bool operator!=(const orv_communication_pixel_format_t& p1, const orv_communication_pixel_format_t& p2);

inline bool OrvVncClient::isSharedAccess() const
{
    return mSharedAccess;
}


} // namespace vnc
} // namespace openrv

#endif

