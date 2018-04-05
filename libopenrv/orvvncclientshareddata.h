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

#ifndef OPENRV_ORVVNCCLIENTSHAREDDATA_H
#define OPENRV_ORVVNCCLIENTSHAREDDATA_H

#include <libopenrv/libopenrv.h>
#include "rfbtypes.h"

#include <mutex>
#include <condition_variable>
#include <list>

/**
 * @file orvvncclientshareddata.h
 *
 * Definition of the @ref OrvVncClientSharedData class and related types that are used by both the @ref
 * OrvVncClient and the connection thread created by the @ref OrvVncClient.
 **/


namespace openrv {
namespace vnc {

enum class ConnectionState
{
    NotConnected = 0,
    /**
     * Set by OrvVncClient to initiate a connection.
     *
     * Only valid if previous state was @ref NotConnected.
     **/
    StartConnection,

    /**
     * Set internally by the openrv::vnc::ConnectionThread when the @ref StartConnection state was
     * encountered and has been handled.
     **/
    ConnectionPending,
    Connected,
};

struct ConnectionInfo
{
    /**
     * Highest protocol version supported by both, client and server.
     **/
    ProtocolVersion mSelectedProtocolVersion = ProtocolVersion::Unknown;
    char mSelectedProtocolVersionString[ORV_VNC_PROTOCOL_VERSION_STRING_LENGTH + 1] = {};
    /**
     * The @re SecurityType that has been selected by the client from the set of types supported
     * by the server.
     **/
    SecurityType mSelectedVNCSecurityType = SecurityType::Invalid;
    //orv_vnc_tight_capability_t mSelectedTightAuthType; // only valid if mSelectedVNCSecurityType==SecurityType::Tight // TODO
    //orv_vnc_tight_capability_t mSelectedTightTunnelCapability; // only valid if mSelectedVNCSecurityType==SecurityType::Tight // TODO

    orv_communication_pixel_format_t mDefaultPixelFormat; // as reported by server on initial connect
    uint16_t mDefaultFramebufferWidth = 0;
    uint16_t mDefaultFramebufferHeight = 0;

    /**
     * Name of the desktop as reported by the server.
     **/
    char* mDesktopName = nullptr;

public:
    ConnectionInfo()
    {
        orv_communication_pixel_format_reset(&mDefaultPixelFormat);
    }
    ~ConnectionInfo()
    {
        free(mDesktopName);
    }
    void reset()
    {
        free(mDesktopName);
        *this = ConnectionInfo();
    }
    void copy(ConnectionInfo* dst);
};

/**
 * Internal helper struct that stores information about an event the client sends to the server.
 * Normally pointer or key events.
 **/
struct ClientSendEvent
{
    enum class Type
    {
        Invalid,
        Pointer,
        Key,
    };

    // for type==Type::Pointer
    ClientSendEvent(Type type, uint16_t x, uint16_t y, uint8_t buttonMask)
        : mType(type),
          mX(x),
          mY(y),
          mButtonMask(buttonMask),
          mDown(false),
          mKey(0)
    {
    }

    // for type==Type::Key
    ClientSendEvent(Type type, bool down, uint32_t key)
        : mType(type),
          mX(0),
          mY(0),
          mButtonMask(0),
          mDown(down),
          mKey(key)
    {
    }

    Type mType = Type::Invalid;

    // Pointer events:
    uint16_t mX = 0;
    uint16_t mY = 0;
    uint8_t mButtonMask = 0;

    // Key events:
    bool mDown = false;
    uint32_t mKey = 0;
};

/**
 * Helper struct for @ref OrvVncClient and the connection thread to store information for a single
 * framebuffer update request.
 **/
struct RequestFramebuffer
{
    RequestFramebuffer() = default;
    RequestFramebuffer(bool incremental, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
        : mIncremental(incremental),
          mX(x),
          mY(y),
          mW(w),
          mH(h)
    {
    }
    bool mIncremental = true;
    uint16_t mX = 0;
    uint16_t mY = 0;
    uint16_t mW = 0;
    uint16_t mH = 0;
};
bool operator==(const RequestFramebuffer& p1, const RequestFramebuffer& p2);
bool operator!=(const RequestFramebuffer& p1, const RequestFramebuffer& p2);

/**
 * Data of @ref OrvVncClient shared between the @ref OrvVncClient and the connection thread that the @ref
 * OrvVncClient controls.
 *
 * All data in this struct is protected by @p mMutex.
 **/
struct OrvVncClientSharedData
{
    mutable std::mutex mMutex;
    std::condition_variable mStartupWaitCondition; // Used on thread startup only
    bool mWantQuitThread = false; // NOTE: If set to true, mAbortFlag must be set to true as well!
    bool mUserRequestedDisconnect = false;
    bool mAbortFlag = false;      // Set to true if user requested disconnect, or if thread is being finished. Both are handled as user-requested disconnect.
    ConnectionState mState = ConnectionState::NotConnected;
    char mHostName[ORV_MAX_HOSTNAME_LEN + 1] = {};
    uint16_t mPort = 0;
    char* mPassword = nullptr;
    size_t mPasswordLength = 0;
    orv_communication_quality_profile_t mRequestQualityProfile = ORV_COMM_QUALITY_PROFILE_SERVER;
    orv_communication_pixel_format_t mRequestFormat;
    bool mWantSendRequestFormat = false;
    bool mWantSendFramebufferUpdateRequest = false;
    std::list<ClientSendEvent> mClientSendEvents;
    RequestFramebuffer mRequestFramebuffer;
    orv_framebuffer_t mFramebuffer; // NOTE: normally NOT the same bpp format as sent by server (we normally use RGB internally only)
    orv_cursor_t mCursorData;
    bool mHaveFramebufferUpdateResponse = false;

    /**
     * Copy of @ref openrv::vnc::ConnectionThread::mServerCapabilities, so that other threads can query
     * it.
     *
     * Only valid if @ref mState is @ref ConnectionState::mConnected.
     **/
    orv_vnc_server_capabilities_t mServerCapabilities;
    /**
     * Copy of @ref openrv::vnc::ConnectionThread::mConnectionInfo, so that other threads can
     * query it.
     *
     * ALL data in this object is meant to be kept in sync with corresponding data in @ref
     * openrv::vnc::ConnectionThread.
     *
     * Only valid if @ref mState is @ref ConnectionState::mConnected.
     **/
    ConnectionInfo mConnectionInfo;
    /**
     * Copy of @ref openrv::vnc::ConnectionThread::mCurrentPixelFormat, so that other threads can
     * query it.
     *
     * Only valid if @ref mState is @ref ConnectionState::mConnected.
     **/
    orv_communication_pixel_format_t mCommunicationPixelFormat;
    /**
     * Copy of @ref openrv::vnc::ConnectionThread::mReceivedBytes
     *
     * This value is not synced after every recv() call, so it may lag behind a little bit.
     **/
    size_t mReceivedBytes = 0;
    /**
     * Copy of @ref openrv::vnc::ConnectionThread::mSentBytes
     *
     * This value is not synced after every send() call, so it may lag behind a little bit.
     **/
    size_t mSentBytes = 0;

public:
    OrvVncClientSharedData();
    virtual ~OrvVncClientSharedData();
    void clearPasswordMutexLocked();
};

} // namespace vnc
} // namespace openrv

#endif

