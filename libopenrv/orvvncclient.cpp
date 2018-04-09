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

// __STDC_FORMAT_MACROS (used in inttypes.h) should be deprecated and not be needed, however current
// android/ndk inttypes.h requires it when using inttypes.h in c++
#define __STDC_FORMAT_MACROS 1

#include "orvvncclient.h"
#include "orvclientdefines.h"
#include "orvvncclientshareddata.h"
#include <libopenrv/orv_logging.h>
#include <libopenrv/libopenrv.h>
#include <libopenrv/orv_error.h>
#include "reader.h"
#include "writer.h"
#include "orv_context.h"
#include "securitytypehandler.h"
#include "messageparser.h"
#include "utils.h"
#include "socket.h"
#include "threadnotifier.h"
#include "rfb3xhandshake.h"
#if defined(OPENRV_HAVE_MBEDTLS)
#include "mbedtlscontext.h"
#endif // OPENRV_HAVE_MBEDTLS
#if defined(OPENRV_HAVE_OPENSSL)
#include "opensslcontext.h"
#endif // OPENRV_HAVE_OPENSSL

#include <assert.h>
#include <sys/types.h>
#if !defined(_MSC_VER)
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <unistd.h>
#endif // _MSC_VER
#include <algorithm>
#include <list>
#include <string.h>
#include <limits>
#include <inttypes.h>
#include <sstream>

#define UNUSED(x) (void)x

// Socket timeout.
// NOTE: This must be relatively high, e.g. 10 seconds is definitely too low for some
//       (slow/unresponsive) servers/connections.
// TODO: Probably make configurable per-connection
#define ORV_SOCKET_TIMEOUT_SECONDS 120

// We use RGB888 in our internal framebuffer, independent from the format used for communication.
#define ORV_INTERNAL_FRAMEBUFFER_BYTES_PER_PIXEL 3


// TODO: support for Tight tunnels?

// TODO: statistics:
// - number of rectangles per encoding (n in Raw, m in RRE, ...)
//   -> lets us see what encodings are actually in use
// - number of bytes sent per encoding

/**
 * Do not accept more subrectangles than this value from remote. This protects against memory
 * garbage sent by the server or received/read (e.g. due to implementation error) causing the client
 * to allocate huge buffers.
 **/
#define ORV_MAX_RRE_SUBRECTANGLES_COUNT 1000000


namespace openrv {
namespace vnc {

bool operator==(const orv_communication_pixel_format_t& p1, const orv_communication_pixel_format_t& p2)
{
    if (p1.mBitsPerPixel == p2.mBitsPerPixel &&
        p1.mDepth == p2.mDepth &&
        p1.mBigEndian == p2.mBigEndian &&
        p1.mTrueColor == p2.mTrueColor &&
        p1.mColorMax[0] == p2.mColorMax[0] &&
        p1.mColorMax[1] == p2.mColorMax[1] &&
        p1.mColorMax[2] == p2.mColorMax[2] &&
        p1.mColorShift[0] == p2.mColorShift[0] &&
        p1.mColorShift[1] == p2.mColorShift[1] &&
        p1.mColorShift[2] == p2.mColorShift[2] ) {
        return true;
    }
    return false;
}

bool operator!=(const orv_communication_pixel_format_t& p1, const orv_communication_pixel_format_t& p2)
{
    return !(p1 == p2);
}


class ConnectionThread
{
public:
    ConnectionThread(orv_context_t* ctx, ThreadNotifierListener* pipeListener, bool sharedAccess, OrvVncClientSharedData* data);
    ~ConnectionThread();

    static void connectionThreadRun(orv_context_t* ctx, ThreadNotifierListener* pipeListener, bool sharedAccess, OrvVncClientSharedData* communicationData);

protected:
    void run();
    void handleConnectedSocketData(SendRecvSocketError* callAgainType);
    void closeSocket();
    void sendEvent(orv_event_t* event);
    void changeStateMutexLocked(ConnectionState state);
    void allocateFramebufferMutexLocked(orv_error_t* error);

    bool handleStartConnectionState();
    bool handleConnectedState();
    bool startConnection(orv_error_t* error);
    bool startVncProtocol(orv_error_t* error);
    bool startVncProtocolRfb3x(orv_error_t* error);
    void negotiateProtocolVersion(orv_error_t* error);
    void performClientAndServerInit(orv_error_t* error, bool sharedAccess);
    size_t processMessageData(const char* buffer, size_t bufferSize, orv_error_t* error);
    void processMessageBell();
    //void processMessageSetColourMapEntries(MessageParserSetColourMapEntries* msg);
    bool sendSetPixelFormat(orv_error_t* error, const orv_communication_pixel_format_t& format);
    void sendSetEncodings(orv_error_t* error);
    bool sendFramebufferUpdateRequest(orv_error_t* error, bool incremental, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void sendKeyEvent(orv_error_t* error, bool down, uint32_t key);
    void sendPointerEvent(orv_error_t* error, uint16_t x, uint16_t y, uint8_t buttonMask);
    void sendClientCutText(orv_error_t* error, const char* text, uint32_t textLen);
    void disconnectWithError(const orv_error_t& error);
    void abortConnectWithError(const orv_error_t& error, orv_auth_type_t authType = ORV_AUTH_TYPE_UNKNOWN);
    void clearPassword();
    char* allocateThreadNameString() const;
    static bool checkFramebufferSize(uint16_t framebufferWidth, uint16_t framebufferHeight, uint8_t bitsPerPixel, orv_error_t* error);
    static void readPixelFormat(orv_communication_pixel_format_t* p, char* buffer, size_t bufferSize);
    static void writePixelFormat(char* buffer, size_t bufferSize, const orv_communication_pixel_format_t& p);
private:
    orv_context_t* mContext = nullptr;
    ThreadNotifierListener* mPipeListener = nullptr;
    const bool mSharedAccess;
    OrvVncClientSharedData* mCommunicationData = nullptr;
    Socket mSocket;
    static const size_t mMaxReceiveBufferSize = 1*1024*1024;
    char* mReceiveBuffer = nullptr;
    size_t mReceiveBufferOffset = 0;
    orv_vnc_server_capabilities_t mServerCapabilities;
    ConnectionInfo mConnectionInfo;
    orv_communication_pixel_format_t mCurrentPixelFormat;
    uint16_t mCurrentFramebufferWidth = 0;
    uint16_t mCurrentFramebufferHeight = 0;
    size_t mFinishedFramebufferUpdateRequests = 0;

    /**
     * Copy of @ref OrvVncClientSharedData::mPort. Copied on connection start and used internally by this
     * thread only.
     **/
    uint16_t mPort = 0;
    /**
     * Copy of @ref OrvVncClientSharedData::mHostName. Copied on connection start and used internally by
     * this thread only.
     **/
    char mHostName[ORV_MAX_HOSTNAME_LEN + 1] = {};
    char* mPassword = nullptr;
    size_t mPasswordLength = 0;
    MbedTlsContext* mMbedTlsContext = nullptr;
    OpenSSLContext* mOpenSSLContext = nullptr;

    MessageParserBase* mCurrentMessageParser = nullptr;
    MessageParserFramebufferUpdate mMessageFramebufferUpdate;
    MessageParserSetColourMapEntries mMessageSetColourMapEntries;
    MessageParserServerCutText mMessageServerCutText;
};

/**
 * Construct the client. This creates an internal connection thread that will perform the actual
 * communication with the VNC server and wait until that thread has been initialized.
 *
 * If construction fails, an error state is set that can be queried using @ref checkError().
 * If an error state is set by this constructor, the object must not be used for anything anymore
 * and should be discarded immediately.
 **/
OrvVncClient::OrvVncClient(orv_context_t* ctx, orv_error_t* constructionError)
{
    orv_error_reset(constructionError);
    mContext = ctx;
    ORV_DEBUG(mContext, "Constructing client %p", this);
    mCommunicationData = new OrvVncClientSharedData;
    mPipeWriter = new ThreadNotifierWriter();
    ThreadNotifierListener* pipeListener = new ThreadNotifierListener();
    if (!ThreadNotifier::makePipe(mPipeWriter, pipeListener)) {
        delete pipeListener;
        pipeListener = nullptr;
    }
    if (pipeListener) {
        std::unique_lock<std::mutex> lock(mCommunicationData->mMutex);
        mThread = new std::thread(&ConnectionThread::connectionThreadRun, mContext, pipeListener, mSharedAccess, mCommunicationData);

        // Wait for the thread to fully start.
        mCommunicationData->mStartupWaitCondition.wait(lock);
    }
    else {
        ORV_ERROR(mContext, "Failed to create local pipe, client unusable");
        orv_error_set(constructionError, ORV_ERR_GENERIC, 0, "Failed to setup local pipe (no more fds available?), unable to setup connection thread.");
    }
}

OrvVncClient::~OrvVncClient()
{
    ORV_DEBUG(mContext, "Destructing client %p", this);
    mCommunicationData->mMutex.lock();
    mCommunicationData->mWantQuitThread = true;
    mCommunicationData->mAbortFlag = true;
    wakeThread();
    mCommunicationData->mMutex.unlock();
    if (mThread) {
        ORV_DEBUG(mContext, "join...");
        mThread->join();
        ORV_DEBUG(mContext, "joined");
    }
    free(mCommunicationData->mFramebuffer.mFramebuffer);
    mCommunicationData->mFramebuffer.mFramebuffer = nullptr;
    mCommunicationData->clearPasswordMutexLocked();
    delete mPipeWriter;
    mPipeWriter = nullptr;
    delete mThread;
    delete mCommunicationData;
    ORV_DEBUG(mContext, "Destructing client %p done", this);
}

/**
 * @pre The client is not yet connected.
 *
 * Set whether to request shared access to the remote desktop non-shared (exclusive) access. The
 * default is true (shared access).
 **/
void OrvVncClient::setSharedAccess(bool shared)
{
    std::unique_lock<std::mutex> lock(mCommunicationData->mMutex);
    if (mCommunicationData->mState != ConnectionState::NotConnected) {
        ORV_ERROR(mContext, "Cannot set shared access to %s, connection already initiated", shared?"true":"false");
        return;
    }
    mSharedAccess = shared;
}
bool OrvVncClient::setCredentials(const char* user, const char* password)
{
    std::unique_lock<std::mutex> lock(mCommunicationData->mMutex);
    const size_t passwordLength = (password != nullptr) ? strlen(password) : 0;
    if (password && passwordLength > ORV_MAX_PASSWORD_LEN) {
        ORV_ERROR(mContext, "Given password is too long");
        return false;
    }
    if (user && strlen(user) > ORV_MAX_USERNAME_LEN) {
        ORV_ERROR(mContext, "Given username is too long");
        return false;
    }

    mCommunicationData->clearPasswordMutexLocked();
    if (password) {
        mCommunicationData->mPassword = strdup(password);
        mCommunicationData->mPasswordLength = passwordLength;
    }

    free(mCommunicationData->mUser);
    mCommunicationData->mUser = nullptr;
    if (user) {
        mCommunicationData->mUser = strdup(user);
    }
    return true;
}


/**
 * @pre The client is not yet connected.
 *
 * Start to connect (asynchronously) to the specified host.
 *
 * This function is thread-safe.
 *
 * @return TRUE if connection has been initiated, FALSE if this object could not start to connect to
 *         the specified host (e.g. connection already established or pending, or if parameters are
 *         invalid).
 **/
bool OrvVncClient::connectToHost(const char* hostName, uint16_t port, const orv_connect_options_t* options, orv_error_t* error)
{
    mViewOnly = (options->mViewOnly != 0);

    std::unique_lock<std::mutex> lock(mCommunicationData->mMutex);
    if (mCommunicationData->mState != ConnectionState::NotConnected) {
        if (error) {
            orv_error_set(error, ORV_ERR_GENERIC, 0, "Previous connection not finished");
        }
        return false;
    }
    if (!mPipeWriter->isValid()) {
        if (error) {
            orv_error_set(error, ORV_ERR_GENERIC, 0, "Failed to open local pipe. No more fds available?");
        }
        return false;
    }
    size_t len = strlen(hostName);
    if (len > ORV_MAX_HOSTNAME_LEN) {
        if (error) {
            orv_error_set(error, ORV_ERR_GENERIC, 0, "hostname too long");
        }
        return false;
    }
    if (port == 0) {
        if (error) {
            orv_error_set(error, ORV_ERR_GENERIC, 0, "Invalid port");
        }
        return false;
    }
    // NOTE: hostname/port in CommunicationData is used for the connection.
    //       we hold an additional copy in this object, so that the user can access it easily, if
    //       required.
    strncpy(mHostName, hostName, ORV_MAX_HOSTNAME_LEN);
    mHostName[ORV_MAX_HOSTNAME_LEN] = 0;
    strncpy(mCommunicationData->mHostName, hostName, ORV_MAX_HOSTNAME_LEN);
    mCommunicationData->mHostName[ORV_MAX_HOSTNAME_LEN] = 0;
    mPort = port;
    mCommunicationData->mPort = port;
    mCommunicationData->mState = ConnectionState::StartConnection;
    mCommunicationData->mRequestQualityProfile = options->mCommunicationQualityProfile;
    orv_communication_pixel_format_copy(&mCommunicationData->mRequestFormat, &options->mCommunicationPixelFormat);
    mCommunicationData->mAbortFlag = mCommunicationData->mWantQuitThread;
    mCommunicationData->mUserRequestedDisconnect = false;
    ORV_DEBUG(mContext, "Triggering thread to connect to %s:%d", mHostName, (int)mPort);
    wakeThread();
    return true;
}

void OrvVncClient::disconnect()
{
    std::unique_lock<std::mutex> lock(mCommunicationData->mMutex);
    ORV_DEBUG(mContext, "User requested disconnect.");
    mCommunicationData->mUserRequestedDisconnect = true;
    mCommunicationData->mAbortFlag = true;
    wakeThread();
}

bool OrvVncClient::isConnected() const
{
    std::unique_lock<std::mutex> lock(mCommunicationData->mMutex);
    if (mCommunicationData->mState != ConnectionState::NotConnected) {
        return true;
    }
    return false;
}

/**
 * @pre No FramebufferUpdateRequest is pending (either was not sent yet, or a response has been
 *      received for the last request).
 *      Note that if this is not satisfied, the contents of the next FramebufferUpdate response is
 *      undefined and may in fact cause corrupt connections (if the size of the message is no longer
 *      known due to changes bits-per-pixel values)
 *
 * Sent the SetPixelFormat message to the server with the specified format and update the @ref
 * CommunicationData::mWantSendRequestFormat accordingly.
 * The requested format is also stored in @ref CommunicationData:mRequestFormat.
 *
 * The caller is responsible to update framebuffer buffers as-needed and to send a new framebuffer
 * update request.
 **/
// TODO: if we want to add a orv_set_pixel_format() in the API:
//       delay sendSetPixelFormat call until no more FramebufferUpdateRequest messages are pending,
//       otherwise response is undefined!
// TODO: similar function with orv_communication_quality_profile_t instead of format
void OrvVncClient::sendSetPixelFormat(const orv_communication_pixel_format_t& format)
{
    if (!isPixelFormatValidForSend(format)) {
#ifdef __GNUC__
#warning TODO return value/error response for sendSetPixelFormat
#endif
        return;
    }
    if (!format.mTrueColor) {
#ifdef __GNUC__
#warning TODO return value/error response for sendSetPixelFormat
#endif
        // currently this client does not support colour maps
        return;
    }
    std::unique_lock<std::mutex> lock(mCommunicationData->mMutex);
    if (!mCommunicationData->mWantSendRequestFormat || format != mCommunicationData->mRequestFormat || mCommunicationData->mRequestQualityProfile != ORV_COMM_QUALITY_PROFILE_CUSTOM) {
        mCommunicationData->mWantSendRequestFormat = true;
        orv_communication_pixel_format_copy(&mCommunicationData->mRequestFormat, &format);
        mCommunicationData->mRequestQualityProfile = ORV_COMM_QUALITY_PROFILE_CUSTOM;
        wakeThread();
    }
}

void OrvVncClient::sendFramebufferUpdateRequest(bool incremental)
{
    std::unique_lock<std::mutex> lock(mCommunicationData->mMutex);
    const uint16_t x = 0;
    const uint16_t y = 0;
    const uint16_t w = mCommunicationData->mFramebuffer.mWidth;
    const uint16_t h = mCommunicationData->mFramebuffer.mHeight;
    if (!mCommunicationData->mHaveFramebufferUpdateResponse) {
        // NOTE: some servers seem to track the previous connection state even a new connection is
        //       initiated (or otherwise assume additional data at the client): If incremental=true,
        //       only a partial framebuffer is sent, even for the first request.
        //       So set this to false until we have received at least one full response.
        incremental = false;
    }
    RequestFramebuffer req(incremental, x, y, w, h);
    if (!mCommunicationData->mWantSendFramebufferUpdateRequest || req != mCommunicationData->mRequestFramebuffer) {
        mCommunicationData->mWantSendFramebufferUpdateRequest = true;
        mCommunicationData->mRequestFramebuffer = req;
        wakeThread();
    }
}

void OrvVncClient::sendFramebufferUpdateRequest(bool incremental, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    RequestFramebuffer req(incremental, x, y, w, h);
    std::unique_lock<std::mutex> lock(mCommunicationData->mMutex);
    if (!mCommunicationData->mWantSendFramebufferUpdateRequest || req != mCommunicationData->mRequestFramebuffer) {
        mCommunicationData->mWantSendFramebufferUpdateRequest = true;
        mCommunicationData->mRequestFramebuffer = req;
        wakeThread();
    }
}

/**
 * @param key Keycode as used by the RFB protocol
 **/
void OrvVncClient::sendKeyEvent(bool down, uint32_t key)
{
    std::unique_lock<std::mutex> lock(mCommunicationData->mMutex);
    if (mCommunicationData->mState != ConnectionState::Connected) {
        return;
    }
    mCommunicationData->mClientSendEvents.push_back(ClientSendEvent(ClientSendEvent::Type::Key, down, key));
    wakeThread();
}

void OrvVncClient::sendPointerEvent(int x, int y, uint8_t buttonMask)
{
    std::unique_lock<std::mutex> lock(mCommunicationData->mMutex);
    if (mCommunicationData->mState != ConnectionState::Connected) {
        return;
    }
    x = std::max(x, 0);
    y = std::max(y, 0);
    x = std::min(x, (int)mCommunicationData->mFramebuffer.mWidth);
    y = std::min(y, (int)mCommunicationData->mFramebuffer.mHeight);
    mCommunicationData->mClientSendEvents.push_back(ClientSendEvent(ClientSendEvent::Type::Pointer, x, y, buttonMask));
    wakeThread();
}

void OrvVncClient::getInfo(orv_connection_info_t* info, orv_vnc_server_capabilities_t* capabilities)
{
    orv_connection_info_reset(info);
    orv_vnc_server_capabilities_reset(capabilities);
    std::unique_lock<std::mutex> lock(mCommunicationData->mMutex);
    if (mCommunicationData->mState != ConnectionState::Connected) {
        return;
    }
    if (info) {
        info->mConnected = 1;
        strncpy(info->mHostName, mHostName, ORV_MAX_HOSTNAME_LEN);
        info->mHostName[ORV_MAX_HOSTNAME_LEN] = '\0';
        info->mPort = mPort;
        orv_communication_pixel_format_copy(&info->mCommunicationPixelFormat, &mCommunicationData->mCommunicationPixelFormat);
        if (mCommunicationData->mConnectionInfo.mDesktopName) {
            strncpy(info->mDesktopName, mCommunicationData->mConnectionInfo.mDesktopName, ORV_MAX_DESKTOP_NAME_LENGTH);
            info->mDesktopName[ORV_MAX_DESKTOP_NAME_LENGTH] = '\0';
        }
        else {
            info->mDesktopName[0] = '\0';
        }
        info->mFramebufferWidth = mCommunicationData->mFramebuffer.mWidth;
        info->mFramebufferHeight = mCommunicationData->mFramebuffer.mHeight;
        info->mReceivedBytes = mCommunicationData->mReceivedBytes;
        info->mSentBytes = mCommunicationData->mSentBytes;
        orv_communication_pixel_format_copy(&info->mDefaultPixelFormat, &mCommunicationData->mConnectionInfo.mDefaultPixelFormat);
        info->mDefaultFramebufferWidth = mCommunicationData->mConnectionInfo.mDefaultFramebufferWidth;
        info->mDefaultFramebufferHeight = mCommunicationData->mConnectionInfo.mDefaultFramebufferHeight;
        info->mSelectedVNCSecurityType = (uint8_t)mCommunicationData->mConnectionInfo.mSelectedVNCSecurityType;
        memcpy(info->mSelectedProtocolVersionString, mCommunicationData->mConnectionInfo.mSelectedProtocolVersionString, ORV_VNC_PROTOCOL_VERSION_STRING_LENGTH);
        info->mSelectedProtocolVersionString[ORV_VNC_PROTOCOL_VERSION_STRING_LENGTH] = '\0';
    }
    if (capabilities) {
        orv_vnc_server_capabilities_copy(capabilities, &mCommunicationData->mServerCapabilities);
    }
}

bool OrvVncClient::isViewOnly() const
{
    return mViewOnly;
}

void OrvVncClient::setViewOnly(bool viewOnly)
{
    mViewOnly = viewOnly;
}

const orv_framebuffer_t* OrvVncClient::acquireFramebuffer()
{
    mCommunicationData->mMutex.lock();
    // TODO: if not yet connected (anymore): guarantee we return a NULL pointer
    // FIXME: also decide whether the mutex is immediately released in that case...
    return &mCommunicationData->mFramebuffer;
}

void OrvVncClient::releaseFramebuffer()
{
    mCommunicationData->mMutex.unlock();
}

const orv_cursor_t* OrvVncClient::acquireCursor()
{
    mCommunicationData->mMutex.lock();
    // TODO: if not yet connected (anymore): guarantee we return a NULL pointer
    // FIXME: also decide whether the mutex is immediately released in that case...
    return &mCommunicationData->mCursorData;
}

void OrvVncClient::releaseCursor()
{
    mCommunicationData->mMutex.unlock();
}

void OrvVncClient::makePixelFormat(orv_communication_pixel_format_t* format, int bitsPerPixel)
{
    switch (bitsPerPixel) {
        case 8:
        case 16:
        case 32:
            break;
        default:
            // invalid value, use fallback.
            bitsPerPixel = 8;
            break;
    }
    orv_communication_pixel_format_reset(format);
    format->mBitsPerPixel = bitsPerPixel;
    format->mTrueColor = 1;
    format->mBigEndian = 0;
    switch (bitsPerPixel) {
        case 8:
            // RGB 332
            // See also https://en.wikipedia.org/wiki/8-bit_color for some historic info on what
            // makes sense when encoding color in 8 bit.
            // RGB 332 seems to be a good choice when using "8 bit truecolor" (i.e. when not using
            // a separate palette/color map).
            format->mColorMax[0] = 7;   // red
            format->mColorMax[1] = 7;   // green
            format->mColorMax[2] = 3;   // blue
            format->mColorShift[0] = 5; // red
            format->mColorShift[1] = 2; // green
            format->mColorShift[2] = 0; // blue
            format->mDepth = bitsPerPixel;
            break;
        case 16:
            // RGB 565
            format->mColorMax[0] = 31;   // red
            format->mColorMax[1] = 63;   // green
            format->mColorMax[2] = 31;   // blue
            format->mColorShift[0] = 11; // red
            format->mColorShift[1] = 5;  // green
            format->mColorShift[2] = 0;  // blue
            format->mDepth = bitsPerPixel;
            break;
        case 32:
            format->mDepth = 24;
            format->mColorMax[0] = 255;  // red
            format->mColorMax[1] = 255;  // green
            format->mColorMax[2] = 255;  // blue
            format->mColorShift[0] = 16; // red
            format->mColorShift[1] = 8;  // green
            format->mColorShift[2] = 0;  // blue
            break;
    }
#if 0
    // RGB 10:10:10
    format.mBitsPerPixel = 32;
    format.mDepth = 30;
    format.mColorMax[0] = 1023; // red
    format.mColorMax[1] = 1023; // green
    format.mColorMax[2] = 1023; // blue
    format.mColorShift[0] = 20; // red
    format.mColorShift[1] = 10; // green
    format.mColorShift[2] = 0; // blue
#endif
#if 0
    // palette
    format.mBitsPerPixel = 8;
    format.mDepth = 8;
    format.mTrueColor = false;
#endif
}

void OrvVncClient::makePixelFormat(orv_communication_pixel_format_t* format, const orv_context_t* context, orv_communication_quality_profile_t profile, const orv_communication_pixel_format_t* serverFormat, const orv_communication_pixel_format_t* customFormat)
{
    switch (profile) {
        case ORV_COMM_QUALITY_PROFILE_LOW:
            OrvVncClient::makePixelFormat(format, 8);
            break;
        case ORV_COMM_QUALITY_PROFILE_MEDIUM:
            OrvVncClient::makePixelFormat(format, 16);
            break;
        case ORV_COMM_QUALITY_PROFILE_BEST:
            OrvVncClient::makePixelFormat(format, 32);
            break;
        default:
        case ORV_COMM_QUALITY_PROFILE_SERVER:
            if (!serverFormat->mTrueColor) {
                ORV_DEBUG(context, "Server uses colour map by default, which is not yet supported by this client. Requesting truecolor instead.");
                orv_communication_pixel_format_reset(format);
            }
            else {
                orv_communication_pixel_format_copy(format, serverFormat);
            }
            break;
        case ORV_COMM_QUALITY_PROFILE_CUSTOM:
            orv_communication_pixel_format_copy(format, customFormat);
            if (!OrvVncClient::isPixelFormatValidForReceive(*format)) {
                ORV_WARNING(context, "User requested to use invalid custom pixel format. Requesting alternative format instead.");
                OrvVncClient::makePixelFormat(format, customFormat->mBitsPerPixel);
            }
            if (!format->mTrueColor) {
                ORV_WARNING(context, "User requested to use colour map, which is not yet supported by this client. Requesting truecolor instead.");
                OrvVncClient::makePixelFormat(format, customFormat->mBitsPerPixel);
            }
            break;
    }
}

/**
 * @return Whether the @p format for receiving data, i.e. when @p format was reported by the server.
 *         This function is @em less restrictive than @ref isPixelFormatValidForSend(), it fails
 *         only if @p format might cause invalid memory accesses or invalid buffers being created.
 **/
bool OrvVncClient::isPixelFormatValidForReceive(const orv_communication_pixel_format_t& format)
{
    switch (format.mBitsPerPixel) {
        case 8:
        case 16:
        case 32:
            // RFB protocol requires 8, 16 or 32 bits per pixel, no other values are currently
            // allowed.
            break;
        default:
            return false;
    }
    if (format.mDepth == 0) {
        return false;
    }
    if (format.mDepth > format.mBitsPerPixel) {
        // depth is the "number of useful bits" in mBitsPerPixel (according to RFB protocol
        // description), so it can never be larger than that value.
        return false;
    }
    return true;
}

/**
 * @return Whether @p format is valid when sending @p format to the server.
 *         This function fails if @ref isPixelFormatValidForSend() fails, but may also fail if
 *         inconsistencies are found that would be non-fatal.
 **/
bool OrvVncClient::isPixelFormatValidForSend(const orv_communication_pixel_format_t& format)
{
    if (!isPixelFormatValidForReceive(format)) {
        return false;
    }
    // TODO:
    // - check ColorMax/ColorShift values are valid
    //   -> Shifts must be less than mBitsPerPixel and in fact less than mDepth
    //      (NOTE: they do NOT need to be different from each other)
    return true;
}

const char* OrvVncClient::getClientMessageTypeString(ClientMessage clientMessageType)
{
#define CASE(x) case ClientMessage::x: return #x
    switch (clientMessageType) {
        CASE(SetPixelFormat);
        CASE(SetEncodings);
        CASE(FramebufferUpdateRequest);
        CASE(KeyEvent);
        CASE(PointerEvent);
        CASE(ClientCutText);
        CASE(VMWare127);
        CASE(OLIVECallControl);
        CASE(ColinDeanxvp);
        CASE(PierreOssmanSetDesktopSize);
        CASE(Tight);
        CASE(Gii);
        CASE(VMWare254);
        CASE(AnthonyLiguori);
    }
#undef CASE
    return "UnknownClientMessageType";
}

const char* OrvVncClient::getServerMessageTypeString(ServerMessage serverMessageType)
{
#define CASE(x) case ServerMessage::x: return #x
    switch (serverMessageType) {
        CASE(FramebufferUpdate);
        CASE(SetColourMapEntries);
        CASE(Bell);
        CASE(ServerCutText);
        CASE(VMWare127);
        CASE(OLIVECallControl);
        CASE(ColinDeanxvp);
        CASE(Tight);
        CASE(Gii);
        CASE(VMWare254);
        CASE(AnthonyLiguori);
    }
#undef CASE
    return "UnknownServerMessageType";
}

const char* OrvVncClient::getSecurityTypeString(SecurityType securityType)
{
#define CASE(x) case SecurityType::x: return #x
    switch (securityType) {
        CASE(Invalid);
        CASE(None);
        CASE(VNCAuthentication);
        CASE(RA2);
        CASE(RA2ne);
        CASE(Tight);
        CASE(Ultra);
        case SecurityType::AnonymousTLS: return "TLS"; // The RFB name is "TLS", we use "AnonymousTLS" internally to make clear that this is *not* a generic TLS. TLS normally uses certificates (even if the verification is often omitted), this *cannot* use certificates.
        CASE(VeNCrypt);
        case SecurityType::GTK_VNC_SASL: return "GTK-VNC-SASL";
        case SecurityType::MD5HashAuthentication: return "MD5 Hash Authentication";
        case SecurityType::ColinDeanxvp: return "Colin Dean xvp";
        case SecurityType::LastEntry: // not an actual SecurityType
            break;
        // no default entry: trigger compiler warning on missing entry
    }
#undef CASE
    return "UnknownSecurityType";
}

const char* OrvVncClient::getEncodingTypeString(EncodingType encodingType)
{
#define CASE(x) case EncodingType::x: return #x
    switch (encodingType) {
        CASE(Raw);
        CASE(CopyRect);
        CASE(RRE);
        CASE(Hextile);
        CASE(ZRLE);
        CASE(Cursor);
        CASE(DesktopSize);
        CASE(CoRRE);
        CASE(zlib);
        CASE(tight);
        CASE(zlibhex);
        CASE(TRLE);
        CASE(HitachiZYWRLE);
        CASE(AdamWallingXZ);
        CASE(AdamWallingXZYW);
        CASE(JPEG);
        CASE(JRLE);
        CASE(TightJpegQualityLevel0);
        CASE(TightJpegQualityLevel1);
        CASE(TightJpegQualityLevel2);
        CASE(TightJpegQualityLevel3);
        CASE(TightJpegQualityLevel4);
        CASE(TightJpegQualityLevel5);
        CASE(TightJpegQualityLevel6);
        CASE(TightJpegQualityLevel7);
        CASE(TightJpegQualityLevel8);
        CASE(TightJpegQualityLevel9);
        CASE(LastRect);
        CASE(PointerPosition);
        CASE(XCursor);
        CASE(TightCompressionLevel);
        CASE(gii);
        CASE(popa);
        CASE(PeterAstrandDesktopName);
        CASE(PierreOssmanExtendedDesktopSize);
        CASE(ColinDeanxvp);
        CASE(OLIVECallControl);
        CASE(Fence);
        CASE(ContinuousUpdates);
        CASE(CursorWithAlpha);
        CASE(JpegFineGrainedQualityLevel);
        CASE(JpegSubsamplingLevel);
    }
    return "UnknownEncodingType";
#undef CASE
}

const char* OrvVncClient::getEventTypeString(orv_event_type_t eventType)
{
#define CASE(x) case x: return #x
    switch (eventType) {
        CASE(ORV_EVENT_NONE);
        CASE(ORV_EVENT_THREAD_STARTED);
        CASE(ORV_EVENT_THREAD_ABOUT_TO_STOP);
        CASE(ORV_EVENT_CONNECT_RESULT);
        CASE(ORV_EVENT_DISCONNECTED);
        CASE(ORV_EVENT_CUT_TEXT);
        CASE(ORV_EVENT_FRAMEBUFFER_UPDATED);
        CASE(ORV_EVENT_FRAMEBUFFER_UPDATE_REQUEST_FINISHED);
        CASE(ORV_EVENT_BELL);
        CASE(ORV_EVENT_CURSOR_UPDATED);
        // no default entry to trigger compiler warning
    }
#undef CASE
    return "UnknownEventType";
}

/**
 * @pre The socket has been connected successfully to the remote host
 * @pre The @ref mCommunicationData::mState is @ref ConnectionState::ConnectionPending.
 *
 * Start the VNC protocol. This function performs the negotiation with the remote server on an
 * already established socket.
 *
 * @return TRUE on success (VNC negotiation successful), FALSE if mAbortFlag is TRUE or on error.
 *         If this function returns FALSE, the @p error argument is set accordingly.
 **/
bool ConnectionThread::startVncProtocol(orv_error_t* error)
{
    orv_error_reset(error);

    bool abortFlag = false;
    mCommunicationData->mMutex.lock();
    abortFlag = mCommunicationData->mAbortFlag;
    mCommunicationData->mMutex.unlock();
    if (abortFlag) {
        orv_error_set(error, ORV_ERR_USER_INTERRUPTION, 0, ORV_ERROR_MSG_USER_INTERRUPTION);
        return false;
    }

    negotiateProtocolVersion(error);
    if (error->mHasError) {
        return false;
    }
    mCommunicationData->mMutex.lock();
    mCommunicationData->mHaveFramebufferUpdateResponse = false;
    memcpy(mCommunicationData->mServerCapabilities.mServerProtocolVersionString, mServerCapabilities.mServerProtocolVersionString, ORV_VNC_PROTOCOL_VERSION_STRING_LENGTH);
    mCommunicationData->mServerCapabilities.mServerProtocolVersionString[ORV_VNC_PROTOCOL_VERSION_STRING_LENGTH] = '\0';
    mCommunicationData->mServerCapabilities.mServerProtocolVersionMajor = mServerCapabilities.mServerProtocolVersionMajor;
    mCommunicationData->mServerCapabilities.mServerProtocolVersionMinor = mServerCapabilities.mServerProtocolVersionMinor;
    memcpy(mCommunicationData->mConnectionInfo.mSelectedProtocolVersionString, mConnectionInfo.mSelectedProtocolVersionString, ORV_VNC_PROTOCOL_VERSION_STRING_LENGTH);
    mCommunicationData->mConnectionInfo.mSelectedProtocolVersionString[ORV_VNC_PROTOCOL_VERSION_STRING_LENGTH] = '\0';
    mCommunicationData->mConnectionInfo.mSelectedProtocolVersion = mConnectionInfo.mSelectedProtocolVersion;
    abortFlag = mCommunicationData->mAbortFlag;
    mCommunicationData->mMutex.unlock();
    if (abortFlag) {
        orv_error_set(error, ORV_ERR_USER_INTERRUPTION, 0, ORV_ERROR_MSG_USER_INTERRUPTION);
        return false;
    }

    switch (mConnectionInfo.mSelectedProtocolVersion) {
        case ProtocolVersion::RFB_3_8:
        case ProtocolVersion::RFB_3_7:
        case ProtocolVersion::RFB_3_3:
            return startVncProtocolRfb3x(error);
        case ProtocolVersion::Unknown:
            break;
        // no default entry to trigger a compiler warning
    }
    orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Internal error: Unhandled protocol version %d (%s)", (int)mConnectionInfo.mSelectedProtocolVersion, mConnectionInfo.mSelectedProtocolVersionString);
    return false;
}

/**
 * @pre The @ref mConnectionInfo.mSelectedProtocolVersionString contains a valid version string
 *
 * Continue @ref startVncProtocol() with RFB 3.x version.
 *
 * The caller has to ensure that when this function is called, the protocol version has already been
 * decided on (but not yet sent back to the server).
 *
 * @return TRUE on success (VNC negotiation successful), FALSE if mAbortFlag is TRUE or on error.
 *         If this function returns FALSE, the @p error argument is set accordingly.
 **/
// TODO: probably split this up into > 1 phase, some init protocols/variants may require us to wait
//       for server, especially when we require authentications.
//       -> we need additional connection states for this. easy to do though.
bool ConnectionThread::startVncProtocolRfb3x(orv_error_t* error)
{
    Rfb3xHandshake rfb3x(mContext, mConnectionInfo.mSelectedProtocolVersion, mConnectionInfo.mSelectedProtocolVersionString, mSocket, mPassword, mPasswordLength, mMbedTlsContext, mOpenSSLContext);
    rfb3x.startVncProtocol(&mServerCapabilities, error);
    clearPassword(); // not needed anymore after this point, remove from memory.
    if (error->mHasError) {
        return false;
    }
    mConnectionInfo.mSelectedVNCSecurityType = rfb3x.selectedSecurityType();

    bool abortFlag = false;
    mCommunicationData->mMutex.lock();
    mCommunicationData->mConnectionInfo.mSelectedVNCSecurityType = mConnectionInfo.mSelectedVNCSecurityType;
    mCommunicationData->mServerCapabilities.mSupportedSecurityTypesCount = mServerCapabilities.mSupportedSecurityTypesCount;
    memcpy(mCommunicationData->mServerCapabilities.mSupportedSecurityTypes, mServerCapabilities.mSupportedSecurityTypes, mServerCapabilities.mSupportedSecurityTypesCount * sizeof(uint8_t));
    mCommunicationData->mServerCapabilities.mSupportedEncodingCapabilitiesCount = mServerCapabilities.mSupportedEncodingCapabilitiesCount;
    mCommunicationData->mServerCapabilities.mSupportedEncodingCapabilitiesPartial = mServerCapabilities.mSupportedEncodingCapabilitiesPartial;
    memcpy(mCommunicationData->mServerCapabilities.mSupportedEncodingCapabilities, mServerCapabilities.mSupportedEncodingCapabilities, mServerCapabilities.mSupportedEncodingCapabilitiesCount * sizeof(orv_vnc_tight_capability_t));
    abortFlag = mCommunicationData->mAbortFlag;
    mCommunicationData->mMutex.unlock();
    if (abortFlag) {
        orv_error_set(error, ORV_ERR_USER_INTERRUPTION, 0, ORV_ERROR_MSG_USER_INTERRUPTION);
        return false;
    }

    performClientAndServerInit(error, mSharedAccess);
    if (error->mHasError) {
        return false;
    }
    mCommunicationData->mMutex.lock();
    // retrieve user-provided parameters
    orv_communication_quality_profile_t initialQualityProfile = mCommunicationData->mRequestQualityProfile;
    orv_communication_pixel_format_t initialCustomPixelFormat;
    orv_communication_pixel_format_copy(&initialCustomPixelFormat, &mCommunicationData->mRequestFormat);
    // save current server info as defaults
    orv_communication_pixel_format_copy(&mCommunicationData->mCommunicationPixelFormat, &mCurrentPixelFormat);
    mCommunicationData->mConnectionInfo.mDefaultFramebufferWidth = mConnectionInfo.mDefaultFramebufferWidth;
    mCommunicationData->mConnectionInfo.mDefaultFramebufferHeight = mConnectionInfo.mDefaultFramebufferHeight;
    abortFlag = mCommunicationData->mAbortFlag;
    mCommunicationData->mMutex.unlock();
    if (abortFlag) {
        orv_error_set(error, ORV_ERR_USER_INTERRUPTION, 0, ORV_ERROR_MSG_USER_INTERRUPTION);
        return false;
    }

    // Not technically part of the initialization phase anymore:
    // Send SetPixelFormat and SetEncodings messages to the server.
    // NOTE: may be re-sent at any time, e.g. if the user decides a different format or different
    //       encodings preference should be used (e.g. to tweak the connection)
    orv_communication_pixel_format_t format;
    OrvVncClient::makePixelFormat(&format, mContext, initialQualityProfile, &mConnectionInfo.mDefaultPixelFormat, &initialCustomPixelFormat);

    if (!format.mTrueColor) {
        ORV_DEBUG(mContext, "Colour map was requested, which is not yet supported by this client. Requesting truecolor instead.");
        orv_communication_pixel_format_reset(&format);
    }


    sendSetPixelFormat(error, format);
    if (error->mHasError) {
        return false;
    }
    sendSetEncodings(error);
    if (error->mHasError) {
        return false;
    }

    mCommunicationData->mMutex.lock();
    mCommunicationData->mFramebuffer.mWidth = mCurrentFramebufferWidth;
    mCommunicationData->mFramebuffer.mHeight = mCurrentFramebufferHeight;
    mCommunicationData->mFramebuffer.mBytesPerPixel = ORV_INTERNAL_FRAMEBUFFER_BYTES_PER_PIXEL;
    mCommunicationData->mFramebuffer.mBitsPerPixel = mCommunicationData->mFramebuffer.mBytesPerPixel * 8;
    mCommunicationData->mServerCapabilities = mServerCapabilities;
    orv_vnc_server_capabilities_copy(&mCommunicationData->mServerCapabilities, &mServerCapabilities);
    orv_communication_pixel_format_copy(&mCommunicationData->mCommunicationPixelFormat, &mCurrentPixelFormat);
    free(mCommunicationData->mConnectionInfo.mDesktopName);
    mCommunicationData->mConnectionInfo.mDesktopName = strdup(mConnectionInfo.mDesktopName);
    allocateFramebufferMutexLocked(error);
    if (!error->mHasError) {
        changeStateMutexLocked(ConnectionState::Connected);
    }
    mCommunicationData->mMutex.unlock();

    if (error->mHasError) {
        return false;
    }

    ORV_INFO(mContext, "VNC connection to %s:%d established", mHostName, (int)mPort);
    ORV_INFO(mContext, "Remote Desktop name: '%s', framebuffer sizer: %dx%d", mConnectionInfo.mDesktopName, (int)mCurrentFramebufferWidth, (int)mCurrentFramebufferHeight);
    return true;
}

/**
 * @post On success, the @ref ConnectionInfo::mSelectedProtocolVersion has been set
 *       (but @em NOT copied to corresponding @ref ConnectionInfo in @ref CommunicationData)
 * @post On success, the @ref ConnectionInfo::mSelectedVNCSecurityType has been set
 *       (but @em NOT copied to corresponding @ref ConnectionInfo in @ref CommunicationData)
 *
 * Implement the protocol version negotiation phase of the RFB protocol, however the selected
 * protocol version is NOT yet written to the socket by this function, i.e. the negotiation is not
 * finalized.
 **/
void ConnectionThread::negotiateProtocolVersion(orv_error_t* error)
{
    const size_t maxReceiveLen = 1024;
    static_assert(maxReceiveLen <= mMaxReceiveBufferSize, "Insufficient receive buffer size");

    // TODO: read the first 4 "RFB " bytes in a loop byte-by-byte, so that we can bail out much more
    // quickly (i.e. without waiting for timeout) if remote is NOT a VNC server and sends less than
    // 12 bytes in an initial message.
    if (!mSocket.readDataBlocking(mReceiveBuffer, ORV_VNC_PROTOCOL_VERSION_STRING_LENGTH, error)) {
        switch (error->mErrorCode) {
            case ORV_ERR_NO_ERROR:
                orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Internal error while reading RFB message from remote host, have no error code.");
                break;
            case ORV_ERR_CLOSED_BY_REMOTE:
                orv_error_set(error, ORV_ERR_CONNECT_ERROR_CONNECTION_REFUSED, 1, "Error reading RFB message from remote host, remote closed connection.");
                break;
            case ORV_ERR_USER_INTERRUPTION:
                break;
        }
        return;
    }

    memcpy(mServerCapabilities.mServerProtocolVersionString, mReceiveBuffer, ORV_VNC_PROTOCOL_VERSION_STRING_LENGTH);
    mServerCapabilities.mServerProtocolVersionString[12] = '\0';

    // The RFB version string is 12 bytes, starting with "RFB ", followed by major.minor in decimal
    // representation with leading zeroes, followed by "\n".
    // Any other string is invalid.
    int major = -1;
    int minor = -1;
    if (sscanf(mServerCapabilities.mServerProtocolVersionString, "RFB %03d.%03d\n", &major, &minor) != 2) {
        mServerCapabilities.mServerProtocolVersionMajor = -1;
        mServerCapabilities.mServerProtocolVersionMinor = -1;
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 3, "Invalid protocol version string received from server. Not a VNC server.");
        return;
    }
    if (major == 3 && minor == 5) {
        // NOTE: RFB docs say some clients (incorrectly) reported 3.5 as version, but this
        // should be treated as 3.3.
        ORV_DEBUG(mContext, "Server reports incorrect/obsolete RFB version 3.5, should be treated as 3.3. Assuming server uses 3.3.");
        minor = 3;
    }
    mServerCapabilities.mServerProtocolVersionMajor = major;
    mServerCapabilities.mServerProtocolVersionMinor = minor;

    // NOTE:
    // VNC servers may announce themselves with RFB 3.3 in order to close the connection
    // immediately with an error message (e.g. "too may connection attempts"). The RFB 3.3
    // protocol does not require further client interaction, so all data can be sent by the
    // server immediately.

    // Fallback: RFB 3.3
    mConnectionInfo.mSelectedProtocolVersion = ProtocolVersion::RFB_3_3;
    memcpy(mConnectionInfo.mSelectedProtocolVersionString, "RFB 003.003\n", 12);
    mConnectionInfo.mSelectedProtocolVersionString[12] = '\0';

    if (major < 3 || (major == 3 && minor < 3)) {
        // RFB 3.3 is the smallest known/published version of the RFB protocol. cannot continue.
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 5, "VNC/RFB version (%d.%d) of server not supported.", major, minor);
        return;
    }
    if (major == 3 && minor < 7) {
        mConnectionInfo.mSelectedProtocolVersion = ProtocolVersion::RFB_3_3;
        memcpy(mConnectionInfo.mSelectedProtocolVersionString, "RFB 003.003\n", 12);
        mConnectionInfo.mSelectedProtocolVersionString[12] = '\0';
    }
    else if (major == 3 && minor == 7) {
        mConnectionInfo.mSelectedProtocolVersion = ProtocolVersion::RFB_3_7;
        memcpy(mConnectionInfo.mSelectedProtocolVersionString, "RFB 003.007\n", 12);
        mConnectionInfo.mSelectedProtocolVersionString[12] = '\0';
    }
    else {
        if (major > 3 || minor > 8) {
            ORV_DEBUG(mContext, "Server reports RFB version major=%d, minor=%d, requesting RFB 003.008 instead.", mServerCapabilities.mServerProtocolVersionMajor, mServerCapabilities.mServerProtocolVersionMinor);
        }
        mConnectionInfo.mSelectedProtocolVersion = ProtocolVersion::RFB_3_8;
        memcpy(mConnectionInfo.mSelectedProtocolVersionString, "RFB 003.008\n", 12);
        mConnectionInfo.mSelectedProtocolVersionString[12] = '\0';
    }
    mConnectionInfo.mSelectedProtocolVersionString[12] = '\0';
    ORV_DEBUG(mContext, "Selected RFB version string: %s", mConnectionInfo.mSelectedProtocolVersionString);

    // NOTE: we do NOT yet write the selected version to the socket: In some RFB versions (most
    //       notably RFB 5.0) we can send the next message immediately after the RFB version, so we
    //       can reduce latency by sending both in the same call (and TCP packet)
}

/**
 * @post On success (@p error has no error), @ref mCurrentPixelFormat is set. The caller is
 *       responsible to update @ref CommunicationData::mCommunicationPixelFormat accordingly.
 * @post On success (@p error has no error), @ref ConnectionInfo::mDefaultFramebufferWidth and @ref
 *       ConnectionInfo::mDefaultFramebufferHeight are set.
 *       The caller is responsible to update corresponding values in @ref CommunicationData
 *       accordingly.
 * @post On success (@p error has no error), @ref ConnectionInfo::mDesktopName is set.
 *       The caller is responsible to update the corresponding value in @ref CommunicationData
 *       accordingly.
 *
 * Performs the "ClientInit" and "ServerInit" steps of the RFB protocol. This in particular updates
 * @li The framebuffer size
 * @li The desktop name
 * @li The default pixel format used by the server (the client may request a different one, see @ref
 *     sendSetPixelFormat())
 *
 * When this function returns with success, the corresponding members of this object have been
 * updated and the protocol is fully initialized. Further communication can be started. However
 * normally the client may want to send a SetEncodings and possibly SetPixelFormat message, to make
 * the connection more useful.
 *
 * NOTE: This function updates the member variables of this object for the values mentioned above
 *       (in particular framebuffer size and pixel format), the corresponding values in @ref
 *       CommunicationData are @em not updated by this function, the caller is responsible for doing
 *       this and should normally do this right after this call.
 **/
void ConnectionThread::performClientAndServerInit(orv_error_t* error, bool sharedAccess)
{
    ORV_DEBUG(mContext, "Sending ClientInit message to server");
    uint8_t clientInitMessage[1];
    clientInitMessage[0] = sharedAccess ? 1 : 0;
    if (!mSocket.writeDataBlocking(clientInitMessage, 1, error)) {
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return;
        }
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 2000, "Failed to write ClientInit message to remote server");
        return;
    }
    ORV_DEBUG(mContext, "Receiving ServerInit message from server");
    if (!mSocket.readDataBlocking(mReceiveBuffer, 24, error)) {
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return;
        }
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 2001, "Failed to read ServerInit message from remote server");
        return;
    }
    uint16_t framebufferWidth;
    uint16_t framebufferHeight;
    char pixelFormat[16];
    uint32_t nameLength;
    framebufferWidth = Reader::readUInt16(mReceiveBuffer + 0);
    framebufferHeight = Reader::readUInt16(mReceiveBuffer + 2);
    memcpy(pixelFormat, mReceiveBuffer + 4, 16);
    nameLength = Reader::readUInt32(mReceiveBuffer + 20);
    char name[ORV_MAX_DESKTOP_NAME_LENGTH + 1];
    if ((size_t)nameLength > ORV_MAX_DESKTOP_NAME_LENGTH) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 2010, "Desktop name provided by server required %u bytes, only %d are supported by this client.", (unsigned int)nameLength, (int)ORV_MAX_DESKTOP_NAME_LENGTH);
        return;
    }
    if (!mSocket.readDataBlocking(name, nameLength, error)) {
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return;
        }
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 2011, "Failed to read desktop name from server");
        return;
    }
    mConnectionInfo.mDefaultFramebufferWidth = framebufferWidth;
    mConnectionInfo.mDefaultFramebufferHeight = framebufferHeight;
    readPixelFormat(&mConnectionInfo.mDefaultPixelFormat, pixelFormat, 16);
    if (!OrvVncClient::isPixelFormatValidForReceive(mConnectionInfo.mDefaultPixelFormat)) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 2050, "Server reports invalid pixel format, cannot connect to this server.");
        return;
    }
    orv_communication_pixel_format_copy(&mCurrentPixelFormat, &mConnectionInfo.mDefaultPixelFormat);
    mCurrentFramebufferWidth = mConnectionInfo.mDefaultFramebufferWidth;
    mCurrentFramebufferHeight = mConnectionInfo.mDefaultFramebufferHeight;
    free(mConnectionInfo.mDesktopName);
    mConnectionInfo.mDesktopName = (char*)malloc(nameLength + 1);
    memcpy(mConnectionInfo.mDesktopName, name, nameLength);
    mConnectionInfo.mDesktopName[nameLength] = 0;

    // check framebuffer parameters with both, the communication format and the internal format.
    // both must be in valid ranges, otherwise we reject the connection.
    if (!checkFramebufferSize(mCurrentFramebufferWidth, mCurrentFramebufferHeight, mCurrentPixelFormat.mBitsPerPixel, error)) {
        return;
    }
    if (!checkFramebufferSize(mCurrentFramebufferWidth, mCurrentFramebufferHeight, ORV_INTERNAL_FRAMEBUFFER_BYTES_PER_PIXEL*8, error)) {
        return;
    }

    if (mConnectionInfo.mSelectedVNCSecurityType != SecurityType::Tight) {
        // When not using the "Tight" security type, the server cannot tell us what encodings it
        // supports.
        // Currently only the "Raw" encoding is inherently supported (protocol dictates that server may
        // use it even if client does not advertise it).
        // TODO: Whenever a new encoding type is encountered (via a server message), add it to this
        //       array in the server capabilities! (of both, this and the CommunicationData object)
        mServerCapabilities.mSupportedEncodingCapabilitiesPartial = 1;
        mServerCapabilities.mSupportedEncodingCapabilitiesCount = 1;
        mServerCapabilities.mSupportedEncodingCapabilities[0] = orv_vnc_tight_capability_t{(int32_t)EncodingType::Raw, "STDV", "RAW_____"};
    }
    else {
        // The "Tight" security type extends the ServerInit protocol by a capabilities section.
        if (!mSocket.readDataBlocking(mReceiveBuffer, 8, error)) {
            if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
                return;
            }
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 2040, "Error reading capabilities header in ServerInit message.");
            return;
        }
        uint16_t numberOfServerMessages = Reader::readUInt16(mReceiveBuffer + 0);
        uint16_t numberOfClientMessages = Reader::readUInt16(mReceiveBuffer + 2);
        uint16_t numberOfEncodings = Reader::readUInt16(mReceiveBuffer + 4);
        if (numberOfServerMessages > ORV_MAX_VNC_SERVER_MESSAGE_CAPABILITIES_READ_COUNT ||
                numberOfClientMessages > ORV_MAX_VNC_CLIENT_MESSAGE_CAPABILITIES_READ_COUNT ||
                numberOfEncodings > ORV_MAX_VNC_ENCODING_TYPES_READ_COUNT) {
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 2041, "Error reading capabilities header in ServerInit message. Server reports %d server message capabilities (allowed %d), %d client message capabilities (allowed %d) and %d encodings capabilities (allowed %d)", (int)numberOfServerMessages, (int)ORV_MAX_VNC_SERVER_MESSAGE_CAPABILITIES_READ_COUNT, (int)numberOfClientMessages, (int)ORV_MAX_VNC_CLIENT_MESSAGE_CAPABILITIES_READ_COUNT, (int)numberOfEncodings, (int)ORV_MAX_VNC_ENCODING_TYPES_READ_COUNT);
            return;
        }
        ORV_DEBUG(mContext, "Server reports %d server message capabilities, %d client message capabilities, %d encodings capabilities.", (int)numberOfServerMessages, (int)numberOfClientMessages, (int)numberOfEncodings);
        const uint32_t totalCapabilities = numberOfServerMessages + numberOfClientMessages + numberOfEncodings;
        uint32_t capabilitiesRead = 0;
        while (capabilitiesRead < totalCapabilities) {
            const size_t bytesPerCapability = 16;
            size_t readCapabilities = std::min((size_t)(mMaxReceiveBufferSize / bytesPerCapability), (size_t)(totalCapabilities - capabilitiesRead));
            const size_t expectedBytes = readCapabilities * bytesPerCapability;
            if (!mSocket.readDataBlocking(mReceiveBuffer, expectedBytes, error)) {
                if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
                    return;
                }
                orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 2042, "Failed reading capabilities from server during ServerInit of Tight security type. Expected %d bytes", (int)expectedBytes);
                return;
            }
            for (int capIndexInReceiveBuffer = 0; capIndexInReceiveBuffer < (int)readCapabilities; capIndexInReceiveBuffer++) {
                orv_vnc_tight_capability_t* c = nullptr;
                const int capIndexTotal = capabilitiesRead + capIndexInReceiveBuffer;
                if (capIndexTotal < (int)numberOfServerMessages) {
                    const int capIndexServerMessages = capIndexTotal;
                    if (capIndexServerMessages < ORV_MAX_VNC_SERVER_MESSAGE_CAPABILITIES_STORED_COUNT) {
                        c = mServerCapabilities.mSupportedServerMessageCapabilities + capIndexServerMessages;
                    }
                }
                else if (capIndexTotal < (int)(numberOfServerMessages + numberOfClientMessages)) {
                    const int capIndexClientMessages = capIndexTotal - numberOfServerMessages;
                    if (capIndexClientMessages < ORV_MAX_VNC_CLIENT_MESSAGE_CAPABILITIES_STORED_COUNT) {
                        c = mServerCapabilities.mSupportedClientMessageCapabilities + capIndexClientMessages;
                    }
                }
                else {
                    const int capIndexEncodings = capIndexTotal - numberOfServerMessages - numberOfClientMessages;
                    if (capIndexEncodings < ORV_MAX_VNC_ENCODING_TYPES_STORED_COUNT) {
                        c = mServerCapabilities.mSupportedEncodingCapabilities + capIndexEncodings;
                    }
                }
                if (c) {
                    const char* capabilityBuffer = mReceiveBuffer + capIndexInReceiveBuffer * bytesPerCapability;
                    c->mCode = Reader::readInt32(capabilityBuffer + 0);
                    memcpy(c->mVendor, capabilityBuffer + 4, 4);
                    c->mVendor[4] = '\0';
                    memcpy(c->mSignature, capabilityBuffer + 8, 8);
                    c->mSignature[8] = '\0';
                }
            }
            capabilitiesRead += readCapabilities;
        }
        mServerCapabilities.mSupportedEncodingCapabilitiesPartial = 0;
        mServerCapabilities.mSupportedEncodingCapabilitiesCount = numberOfEncodings;
        mServerCapabilities.mSupportedServerMessageCapabilitiesCount = numberOfServerMessages;
        mServerCapabilities.mSupportedClientMessageCapabilitiesCount = numberOfClientMessages;

        // NOTE: the Raw encoding is always inherently supported.
        if (mServerCapabilities.mSupportedEncodingCapabilitiesCount + 1 < ORV_MAX_VNC_ENCODING_TYPES_STORED_COUNT) {
            bool found = false;
            orv_vnc_tight_capability_t rawEncoding{(int32_t)EncodingType::Raw, "STDV", "RAW_____"};
            for (int i = 0; i < (int)mServerCapabilities.mSupportedEncodingCapabilitiesCount; i++) {
                if (orv_are_capabilities_equal(&rawEncoding, &mServerCapabilities.mSupportedEncodingCapabilities[i])) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                mServerCapabilities.mSupportedEncodingCapabilities[mServerCapabilities.mSupportedEncodingCapabilitiesCount] = rawEncoding;
                mServerCapabilities.mSupportedEncodingCapabilitiesCount++;
            }
        }
    }

    // ensure there is no more data on the socket. after this, the protocol initialization is done.
#if !defined(_MSC_VER)
    int dataOnSocket = 0;
    if (ioctl(mSocket.socketFd(), FIONREAD, &dataOnSocket) != 0)
#else // !_MSC_VER
    u_long dataOnSocket = 0;
    if (ioctlsocket(mSocket.socketFd(), FIONREAD, &dataOnSocket) != 0)
#endif // !_MSC_VER
    {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 2020, "ioctl on socket failed. errno: %d", (int)errno);
        return;
    }
    if (dataOnSocket != 0) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_PROTOCOL_ERROR, 2021, "Server sent unexpected data after ServerInit message. Have %d remaining bytes on socket.", dataOnSocket);
        return;
    }
}

void ConnectionThread::readPixelFormat(orv_communication_pixel_format_t* p, char* buffer, size_t bufferSize)
{
    if (bufferSize < 16) {
        orv_communication_pixel_format_reset(p);
        return;
    }
    p->mBitsPerPixel = Reader::readUInt8(buffer + 0);
    p->mDepth = Reader::readUInt8(buffer + 1);
    p->mBigEndian = (buffer[2] != 0);
    p->mTrueColor = (buffer[3] != 0);
    p->mColorMax[0] = Reader::readUInt16(buffer + 4); // red
    p->mColorMax[1] = Reader::readUInt16(buffer + 6); // green
    p->mColorMax[2] = Reader::readUInt16(buffer + 8); // blue
    p->mColorShift[0] = Reader::readUInt8(buffer + 10); // red
    p->mColorShift[1] = Reader::readUInt8(buffer + 11); // green
    p->mColorShift[2] = Reader::readUInt8(buffer + 12); // blue
    // bytes 13..15 are padding
}

void ConnectionThread::writePixelFormat(char* buffer, size_t bufferSize, const orv_communication_pixel_format_t& p)
{
    if (bufferSize < 16) {
        return;
    }
    Writer::writeUInt8(buffer + 0, p.mBitsPerPixel);
    Writer::writeUInt8(buffer + 1, p.mDepth);
    Writer::writeUInt8(buffer + 2, p.mBigEndian ? 1 : 0);
    Writer::writeUInt8(buffer + 3, p.mTrueColor ? 1 : 0);
    Writer::writeUInt16(buffer + 4, p.mColorMax[0]); // red
    Writer::writeUInt16(buffer + 6, p.mColorMax[1]); // green
    Writer::writeUInt16(buffer + 8, p.mColorMax[2]); // blue
    Writer::writeUInt8(buffer + 10, p.mColorShift[0]); // red
    Writer::writeUInt8(buffer + 11, p.mColorShift[1]); // green
    Writer::writeUInt8(buffer + 12, p.mColorShift[2]); // blue
    Writer::writeUInt8(buffer + 13, 0);
    Writer::writeUInt8(buffer + 14, 0);
    Writer::writeUInt8(buffer + 15, 0);
}

/**
 * Write a byte to the local pipe, signalling the thread to wake up.
 **/
void OrvVncClient::wakeThread()
{
    if (mPipeWriter) {
        mPipeWriter->sendNotification();
    }
}


/**
 * @param pipeListener The read-end of a local pipe. This object takes ownership of the provided
 *        pointer and deletes it on destruction.
 *        Must be non-NULL and must hold a valid fd.
 **/
ConnectionThread::ConnectionThread(orv_context_t* ctx, ThreadNotifierListener* pipeListener, bool sharedAccess, OrvVncClientSharedData* communicationData)
    : mContext(ctx),
      mPipeListener(pipeListener),
      mSharedAccess(sharedAccess),
      mCommunicationData(communicationData),
      mSocket(ctx, pipeListener, communicationData),
      mMessageFramebufferUpdate(ctx, &mCommunicationData->mMutex, &mCommunicationData->mMutex, &mCommunicationData->mFramebuffer, &mCommunicationData->mCursorData, &mCurrentPixelFormat, &mCurrentFramebufferWidth, &mCurrentFramebufferHeight),
      mMessageSetColourMapEntries(ctx),
      mMessageServerCutText(ctx)
{
    ORV_DEBUG(mContext, "Constructing connection thread %p", this);
    orv_communication_pixel_format_reset(&mConnectionInfo.mDefaultPixelFormat);
    mReceiveBuffer = new char[mMaxReceiveBufferSize + 1];

    // NOTE: OpenSSL/mbedtls context objects are created, but not initialized: That is delayed until
    //       they are actually needed.
    //       Also note: if mbedtls/OpenSSL is not available, the pointers remain NULL
#ifdef OPENRV_HAVE_MBEDTLS
    mMbedTlsContext = new MbedTlsContext();
#endif // OPENRV_HAVE_MBEDTLS
#ifdef OPENRV_HAVE_OPENSSL
    mOpenSSLContext = new OpenSSLContext();
#endif // OPENRV_HAVE_OPENSSL
}

ConnectionThread::~ConnectionThread()
{
    ORV_DEBUG(mContext, "Destructing connection thread %p", this);
    mSocket.close();
    delete[] mReceiveBuffer;
    mConnectionInfo.reset();
}

/**
 * @pre Called by connection thread
 * @pre The current connection state is @ref ConnectionState::Connected, i.e. the
 *      connection is fully established.
 * @pre Data is available on the socket to read (at least one byte)
 *
 * @param callAgainType If no data is available on the socket and the caller needs to select() for
 *        more data, this parameter indicates what to select on - read or write.
 *        Noramlly this is always set to @ref SendRecvSocketError::CallAgainWaitForRead, however on
 *        encrypted connections, the read function may actually require to @em write data (if SSL
 *        renegotiation is required), and therefore the caller has to select() on the socket being
 *        writable before calling the read function again.
 *        Note that this value is set to @ref SendRecvSocketError::CallAgainWaitForRead by default,
 *        if no wait is required at all, in which case a select() on the socket being readable will
 *        return immediately anyway.
 *        This value should be ignored on errors.
 **/
void ConnectionThread::handleConnectedSocketData(SendRecvSocketError* callAgainType)
{
    orv_error_t error;
    orv_error_reset_minimal(&error);

    if (mReceiveBufferOffset >= mMaxReceiveBufferSize) {
        orv_error_set(&error, ORV_ERR_READ_FAILED, 99, "Failed to read data from socket, due to internal error: Receive buffer full, should have been cleared.");
        disconnectWithError(error);
        return;
    }
    uint32_t s = mSocket.readAvailableDataNonBlocking(mReceiveBuffer + mReceiveBufferOffset, mMaxReceiveBufferSize - mReceiveBufferOffset, callAgainType, &error);
    if (error.mHasError) {
        ORV_WARNING(mContext, "Failed to read data from the socket, error code: %d.%d, error message: %s", (int)error.mErrorCode, (int)error.mSubErrorCode, error.mErrorMessage);
        disconnectWithError(error);
        return;
    }
    if (s == 0) {
        // no data available, select() for more.
        // NOTE: readAvailableDataNonBlocking() has already set callAgainType as required.
        return;
    }
    *callAgainType = SendRecvSocketError::CallAgainWaitForRead; // next select() should wait for read, write is required in special cases only (SSL renegotiation).
    size_t receiveBufferLen = mReceiveBufferOffset + s;
    size_t offset = 0;
    bool ok = true;
    while (ok && offset < receiveBufferLen) {
        orv_error_t error;
        size_t consumedBytes = processMessageData(mReceiveBuffer + offset, receiveBufferLen - offset, &error);
        //ORV_DEBUG(mContext, "Processed message data in buffer. Offset=%d, receiveBufferLen=%d, consumed=%d, error=%d", (int)offset, (int)receiveBufferLen, (int)consumedBytes, (int)error.mHasError);
        if (error.mHasError) {
            ORV_DEBUG(mContext, "Disconnecting due to error in processMessageData");
            disconnectWithError(error);
            return;
        }
        if (consumedBytes == 0) {
            // wait for more data
            ok = false;
        }
        else {
            offset += consumedBytes;
        }
    }
    if (offset > receiveBufferLen) {
        ORV_ERROR(mContext, "Buffer offset %u exceeds buffer contents length %u", (unsigned int)offset, (unsigned int)receiveBufferLen);
        orv_error_t error;
        orv_error_set(&error, ORV_ERR_GENERIC, 0, "Buffer offset %u exceeds buffer contents length %u", (unsigned int)offset, (unsigned int)receiveBufferLen);
        disconnectWithError(error);
        return;
    }

    if (offset < receiveBufferLen && offset > 0) {
        //ORV_DEBUG(mContext, "Remaining data in buffer for next iteration: %d", (int)(receiveBufferLen - offset));
        memmove(mReceiveBuffer, mReceiveBuffer + offset, receiveBufferLen - offset);
    }
    mReceiveBufferOffset = receiveBufferLen - offset;
    //ORV_WARNING(mContext, "TODO: %s", __PRETTY_FUNCTION__);
    //ORV_DEBUG(mContext, "Read %d bytes from socket", (int)s);
    //for (int i = 0; i < (int)s ; i++) {
    //    ORV_DEBUG(mContext, "%02d: %x (%d)", i, mReceiveBuffer[i], mReceiveBuffer[i]);
    //}
}

/**
 * @return The number of bytes consumed from @p buffer.
 *         If more data is required, 0 is returned.
 **/
size_t ConnectionThread::processMessageData(const char* buffer, size_t bufferSize, orv_error_t* error)
{
    orv_error_reset(error);
    if (bufferSize == 0) {
        return 0;
    }
    if (!mCurrentMessageParser) {
        uint8_t messageType = buffer[0];
        ORV_DEBUG(mContext, "Have new message of type %d (%s)", (int)messageType, OrvVncClient::getServerMessageTypeString((ServerMessage)messageType));
        uint32_t offset = 1;
        switch ((ServerMessage)messageType) {
            case ServerMessage::FramebufferUpdate:
                mCurrentMessageParser = &mMessageFramebufferUpdate;
                break;
            case ServerMessage::SetColourMapEntries:
                mCurrentMessageParser = &mMessageSetColourMapEntries;
                break;
            case ServerMessage::Bell:
                // message already complete
                processMessageBell();
                return offset;
            case ServerMessage::ServerCutText:
                mCurrentMessageParser = &mMessageServerCutText;
                break;
            default:
                ORV_ERROR(mContext, "Unexpected message type %d, cannot handle message. Protocol error.", (int)messageType);
                orv_error_set(error, ORV_ERR_PROTOCOL_ERROR, 100, "Unexpected message type %d, cannot handle message.", (int)messageType);
                mCurrentMessageParser = nullptr;
                return 0;
        }
        mCurrentMessageParser->reset();
    }
    ORV_DEBUG(mContext, "processing data for message type %d (%s), bytes in buffer: %d", (int)mCurrentMessageParser->messageType(), mCurrentMessageParser->messageTypeString(), (int)bufferSize);
    uint32_t consumed = mCurrentMessageParser->readData(buffer, bufferSize, error);
    if (error->mHasError) {
        mCurrentMessageParser->reset();
        mCurrentMessageParser = nullptr;
        return 0;
    }

    // TODO: add a processPartialMessage() function that can send events for partial messages?
    //       -> for FramebufferUpdate messages, we want to send events for all finished rects, even
    //          before the full message has been received

    if (mCurrentMessageParser->isFinished()) {
        ORV_DEBUG(mContext, "message type %d (%s) completed", (int)mCurrentMessageParser->messageType(), mCurrentMessageParser->messageTypeString());
        orv_event_t* e = mCurrentMessageParser->processFinishedMessage(error);
        mCurrentMessageParser = nullptr;
        if (error->mHasError) {
            if (e) {
                orv_event_destroy(e);
            }
            return 0;
        }
        if (e) {
            if (e->mEventType == ORV_EVENT_FRAMEBUFFER_UPDATE_REQUEST_FINISHED) {
                if (mFinishedFramebufferUpdateRequests == 0) {
                    mCommunicationData->mMutex.lock();
                    mCommunicationData->mHaveFramebufferUpdateResponse = true;
                    mCommunicationData->mMutex.unlock();
                }
                mFinishedFramebufferUpdateRequests++;
                if (mFinishedFramebufferUpdateRequests == 1 || (mFinishedFramebufferUpdateRequests % 100 == 0)) {
                    ORV_DEBUG(mContext, "Finished %d framebuffer update requests up until now. Received bytes so far: %d, sent: %d", (int)mFinishedFramebufferUpdateRequests, (int)mSocket.receivedBytes(), (int)mSocket.sentBytes());
                }
            }
            sendEvent(e);
        }
    }
    return consumed;
}

void ConnectionThread::processMessageBell()
{
    orv_event_t* event = orv_event_init(ORV_EVENT_BELL);
    sendEvent(event);
}

void ConnectionThread::connectionThreadRun(orv_context_t* ctx, ThreadNotifierListener* pipeListener, bool sharedAccess, OrvVncClientSharedData* communicationData)
{
    ConnectionThread threadObject(ctx, pipeListener, sharedAccess, communicationData);
    threadObject.run();
}

/**
 * @post The Socket is closed
 * @pre The @ref mMutex is NOT locked
 *
 * Internal function to close the internal socket fd.
 *
 * This function does @em not perform a sane connection close, it merely closes the socket directly.
 * Any close communication required by the protocol must have been performed already.
 **/
void ConnectionThread::closeSocket()
{
#ifdef OPENRV_HAVE_OPENSSL
    if (mOpenSSLContext) {
        mOpenSSLContext->shutdown();
    }
#endif // OPENRV_HAVE_OPENSSL
#ifdef OPENRV_HAVE_MBEDTLS
    if (mOpenSSLContext) {
        mMbedTlsContext->shutdown();
    }
#endif // OPENRV_HAVE_MBEDTLS
    mSocket.clearEncryptionContext();
#ifdef OPENRV_HAVE_MBEDTLS
    mMbedTlsContext = new MbedTlsContext();
    delete mMbedTlsContext;
#endif // OPENRV_HAVE_MBEDTLS
#ifdef OPENRV_HAVE_OPENSSL
    delete mOpenSSLContext;
    mOpenSSLContext = new OpenSSLContext();
#endif // OPENRV_HAVE_OPENSSL
    mSocket.close();
    mReceiveBufferOffset = 0;
    orv_vnc_server_capabilities_reset(&mServerCapabilities);
    mConnectionInfo.reset();
    mMessageFramebufferUpdate.resetConnection();
}

/**
 * Send @p event using the event callback.
 *
 * This function takes ownership of @p event.
 **/
void ConnectionThread::sendEvent(orv_event_t* event)
{
    mContext->mConfig.mEventCallback(mContext, event);
}

/**
 * Main thread function. This function remains active until the thread is exited (and normally the
 * object destroyed).
 *
 * On entry, the @ref mCommunicationData::mMutex is LOCKED. This function unlocks the mutex
 * immediately, signalling the caller that the object has been fully created.
 **/
void ConnectionThread::run()
{
    ORV_DEBUG(mContext, "Entering connection %p thread main function", this);

    {
        orv_event_t* e = orv_event_init(ORV_EVENT_THREAD_STARTED);
        e->mEventData = allocateThreadNameString();
        // notify the event callback that the thread has started, so that thread-specific data can
        // be initialized, if required.
        sendEvent(e);
    }

    // signal the client that this thread is now fully usable.
    mCommunicationData->mMutex.lock();
    mCommunicationData->mStartupWaitCondition.notify_all();
    mCommunicationData->mMutex.unlock();

    // Only used when connectionState is Connected.
    // Normally we select() for the socket being readable. However occasionally we may actually
    // require to wait for the socket being *writable* (during SSL renegotiation on encrypted
    // connections). In that case, this value is set accordingly, so the next select() call waits
    // for the socket being writable instead.
    // On unencrypted connections this is always CallAgainWaitForRead.
    SendRecvSocketError nextSelectSocketCallAgainType = SendRecvSocketError::CallAgainWaitForRead;

    bool wantQuitThread = false;
    while (!wantQuitThread) {
        //bool wantDisconnect = false;
        bool abortFlag = false;
        mCommunicationData->mMutex.lock();
        // NOTE: abortFlag is set either on mUserRequestedDisconnect or on mWantQuitThread, both are
        //       handled as user-requested disconnect.
        //       mUserRequestedDisconnect is *set* in case we need it, but actually checking it is
        //       optional - checking mAbortFlag is *mandatory*.
        wantQuitThread = mCommunicationData->mWantQuitThread;
        //wantDisconnect = mCommunicationData->mUserRequestedDisconnect;
        abortFlag = mCommunicationData->mAbortFlag;
        ConnectionState connectionState = mCommunicationData->mState;
        mCommunicationData->mReceivedBytes = mSocket.receivedBytes();
        mCommunicationData->mSentBytes = mSocket.sentBytes();
        mCommunicationData->mMutex.unlock();
        if (wantQuitThread) {
            break;
        }
        if (abortFlag) {
            switch (connectionState) {
                case ConnectionState::ConnectionPending: // should not be reached
                case ConnectionState::StartConnection:
                {
                    orv_error_t error;
                    orv_error_set(&error, ORV_ERR_USER_INTERRUPTION, 0, ORV_ERROR_MSG_USER_INTERRUPTION);
                    abortConnectWithError(error);
                    continue;
                }
                case ConnectionState::NotConnected: // NOTE: Make sure we send the disconnected event even if we were not yet connected.
                case ConnectionState::Connected:
                    break;
                // no default entry to trigger a compiler warning
            }
            orv_error_t error;
            orv_error_set(&error, ORV_ERR_USER_INTERRUPTION, 0, ORV_ERROR_MSG_USER_INTERRUPTION);
            disconnectWithError(error);
            continue;
        }
        bool connectionStateHandled = false;
        bool doSelect = false;
        bool selectForSocket = false;
        switch (connectionState) {
            case ConnectionState::ConnectionPending:
            {
                // should not be reached, should be completely handled by StartConnection currently
                // (this may change)
                orv_error_t error;
                orv_error_set(&error, ORV_ERR_GENERIC, 0, "Unexpected ConnectionState %d, should not be reached.", (int)connectionState);
                abortConnectWithError(error);
                break;
            }
            case ConnectionState::NotConnected:
                // nothing to do. select() until the controlling thread wakes us up.
                connectionStateHandled = true;
                doSelect = true;
                selectForSocket = false;
                break;
            case ConnectionState::StartConnection:
            {
                connectionStateHandled = true;
                if (handleStartConnectionState()) {
                    doSelect = false;
                }
                break;
            }
            case ConnectionState::Connected:
                // Main event loop state.
                // Here we send any pending messages to the server, wait for data and process data
                // received from the server.
                connectionStateHandled = true;
                if (handleConnectedState()) {
                    doSelect = true;
                    selectForSocket = true;
                }
                break;
            // no default entry, to trigger a compiler warning
        }
        if (!connectionStateHandled) {
            ORV_ERROR(mContext, "Internal error: unhandled connection state %d", (int)connectionState);
            orv_error_t error;
            orv_error_set(&error, ORV_ERR_GENERIC, 0, "Internal error: unhandled connection state %d", (int)connectionState);
            disconnectWithError(error);
        }
        else if (doSelect) {
            // sync sent/received bytes prior to waiting for data, in case the wait takes longer.
            mCommunicationData->mMutex.lock();
            mCommunicationData->mReceivedBytes = mSocket.receivedBytes();
            mCommunicationData->mSentBytes = mSocket.sentBytes();
            mCommunicationData->mMutex.unlock();

            int lastError = 0;
            const bool useTimeout = false;
            bool signalledSocket = false;
            Socket::WaitType waitType = Socket::WaitType::NoSocketWait;
            if (selectForSocket) {
                if (nextSelectSocketCallAgainType == SendRecvSocketError::CallAgainWaitForWrite) {
                    waitType = Socket::WaitType::Write;
                }
                else {
                    waitType = Socket::WaitType::Read;
                }
            }
            Socket::WaitRet waitRet = mSocket.waitForSignal(0, 0, useTimeout, waitType, &lastError, &signalledSocket);
            nextSelectSocketCallAgainType = SendRecvSocketError::CallAgainWaitForRead; // current wait finished, next wait is for read, unless proven otherwise.
            switch (waitRet) {
                case Socket::WaitRet::Error:
                {
                    ORV_DEBUG(mContext, "select() returned error lastError: %d", lastError);
                    orv_error_t error;
                    if (selectForSocket) {
                        orv_error_set(&error, ORV_ERR_GENERIC, 0, "Error while waiting for data on socket, lastError=%d", lastError);
                    }
                    else {
                        orv_error_set(&error, ORV_ERR_GENERIC, 0, "Error while waiting for signal, lastError=%d", lastError);
                    }
                    disconnectWithError(error);
                    break;
                }
                case Socket::WaitRet::Timeout:
                    // we ignore this case and simply return to the select loop.
                    break;
                case Socket::WaitRet::UserInterruption:
                    break;
                case Socket::WaitRet::Signalled:
                {
                    if (selectForSocket && signalledSocket) {
                        switch (connectionState) {
                            case ConnectionState::Connected:
                                handleConnectedSocketData(&nextSelectSocketCallAgainType);
                                break;
                            default:
                            {
                                ORV_ERROR(mContext, "Called select() on socket in unexpected connectionState %d", (int)connectionState);
                                orv_error_t error;
                                orv_error_set(&error, ORV_ERR_GENERIC, 0, "Internal error: Called select() on socket in unexpected connectionState %d", (int)connectionState);
                                disconnectWithError(error);
                                break;
                            }
                        }
                    }
                    break;
                }
            }
        }
    }

    {
        // notify the event callback that the thread is about to be stopped, so that thread-specific
        // data can be deleted, if required.
        orv_event_t* e = orv_event_init(ORV_EVENT_THREAD_ABOUT_TO_STOP);
        e->mEventData = allocateThreadNameString();
        sendEvent(e);
    }

    ORV_DEBUG(mContext, "Leaving connection %p thread main function", this);
}
static orv_auth_type_t authTypeFromVncSecurityType(SecurityType securityType)
{
    orv_auth_type_t authType = ORV_AUTH_TYPE_UNKNOWN;
    switch (securityType)
    {
    case SecurityType::None:
        authType = ORV_AUTH_TYPE_NONE;
        break;
    case SecurityType::VNCAuthentication:
        authType = ORV_AUTH_TYPE_VNC;
        break;
    default:
        authType = ORV_AUTH_TYPE_UNKNOWN;
        break;
    }
    return authType;
}

/**
 * @pre The current @ref mState is @ref ConnectionState::StartConnection
 *
 * Called internally to handle th @ref ConnectionState::StartConnection state.
 * This function initiates the actual connection (see @ref startConnection()) and will process it
 * accordingly.
 *
 * This function does @em not wait for data on the socket and does not read new data from the
 * socket - receiving data is handled separately from this function.
 *
 * @return TRUE on success, FALSE on error. If this function returns FALSE, the connection has been
 *         aborted and both a @ref ORV_EVENT_CONNECT_RESULT and a @ref ORV_EVENT_DISCONNECTED
 *         event have been sent.
 *         If this function returns TRUE, the connection has been established and the @ref
 *         ORV_EVENT_CONNECT_RESULT event has been sent.
 **/
bool ConnectionThread::handleStartConnectionState()
{
    orv_error_t error;
    bool ok = startConnection(&error);
    if (!ok || error.mHasError) {
        if (!error.mHasError) {
            ORV_WARNING(mContext, "startConnection call returned false, but did not set error parameter. Using fallback error message.");
            orv_error_set(&error, ORV_ERR_CONNECT_ERROR_GENERIC, 100001, "Failed to connect to remote host with unknown error (unhandled error condition).");
        }
        abortConnectWithError(error);
        return false;
    }
    ok = startVncProtocol(&error);
    if (!ok || error.mHasError) {
        if (!error.mHasError) {
            ORV_WARNING(mContext, "startVncProtocol call returned false, but did not set error parameter. Using fallback error message.");
            orv_error_set(&error, ORV_ERR_CONNECT_ERROR_GENERIC, 100002, "Failed initialize VNC protocol with remote host with unknown error (unhandled error condition).");
        }
        abortConnectWithError(error, authTypeFromVncSecurityType(mConnectionInfo.mSelectedVNCSecurityType));
        return false;
    }
    orv_event_t* event = orv_event_connect_result_init(mHostName, mPort, mCurrentFramebufferWidth, mCurrentFramebufferHeight, mConnectionInfo.mDesktopName, &mCurrentPixelFormat, authTypeFromVncSecurityType(mConnectionInfo.mSelectedVNCSecurityType), nullptr);
    sendEvent(event);
    return true;
}

/**
 * @pre The current @ref mState is @ref ConnectionState::Connected
 *
 * Called internally by the main thread loop to handle the "Connected" state. This function checks
 * if any actions have to be performed and acts accordingly.
 *
 * This function does @em not wait for data on the socket and does not read new data from the
 * socket - receiving data is handled separately from this function.
 *
 * @return FALSE on error, otherwise TRUE. If this function returns FALSE, the connection has been
 *         closed and a @ref ORV_EVENT_DISCONNECTED event has been sent.
 **/
bool ConnectionThread::handleConnectedState()
{
    std::unique_lock<std::mutex> lock(mCommunicationData->mMutex);
    orv_communication_pixel_format_t requestFormat;
    if (mCommunicationData->mWantSendRequestFormat && mCommunicationData->mRequestQualityProfile == ORV_COMM_QUALITY_PROFILE_CUSTOM) {
        orv_communication_pixel_format_copy(&requestFormat, &mCommunicationData->mRequestFormat);
    }
    orv_communication_quality_profile_t qualityProfile = mCommunicationData->mRequestQualityProfile;
    bool wantSendRequestFormat = mCommunicationData->mWantSendRequestFormat;
    mCommunicationData->mWantSendRequestFormat = false;
    bool wantSendFramebufferUpdateRequest = mCommunicationData->mWantSendFramebufferUpdateRequest;
    mCommunicationData->mWantSendFramebufferUpdateRequest = false;
    RequestFramebuffer framebufferUpdateRequest = mCommunicationData->mRequestFramebuffer;
    std::list<ClientSendEvent> sendEvents;
    std::swap(sendEvents, mCommunicationData->mClientSendEvents);
    lock.unlock();
    if (wantSendRequestFormat) {
        orv_error_t error;
        orv_error_reset(&error);
        orv_communication_pixel_format_t format;
        OrvVncClient::makePixelFormat(&format, mContext, qualityProfile, &mConnectionInfo.mDefaultPixelFormat, &requestFormat);
        if (!sendSetPixelFormat(&error, format)) {
            disconnectWithError(error);
            return false;
        }
    }
    if (wantSendFramebufferUpdateRequest) {
        orv_error_t error;
        orv_error_reset(&error);
        if (!sendFramebufferUpdateRequest(&error, framebufferUpdateRequest.mIncremental, framebufferUpdateRequest.mX, framebufferUpdateRequest.mY, framebufferUpdateRequest.mW, framebufferUpdateRequest.mH)) {
            disconnectWithError(error);
            return false;
        }
    }
    if (!sendEvents.empty()) {
        orv_error_t error;
        orv_error_reset(&error);
        while (!sendEvents.empty()) {
            const ClientSendEvent& e = sendEvents.front();
            switch (e.mType) {
                case ClientSendEvent::Type::Key:
                    sendKeyEvent(&error, e.mDown, e.mKey);
                    break;
                case ClientSendEvent::Type::Pointer:
                    sendPointerEvent(&error, e.mX, e.mY, e.mButtonMask);
                    break;
                case ClientSendEvent::Type::Invalid:
                    break;
            }
            sendEvents.pop_front();
            if (error.mHasError) {
                disconnectWithError(error);
                return false;
            }
        }
    }
    return true;
}

/**
 * @pre The @ref mMutex is locked
 * @pre @ref mCommunicationData::mState is @ref ConnectionState::StartConnection
 * @pre @ref mCommunicationData::mPort is valid
 * @pre @ref mCommunicationData::mHostName is valid
 * @post the @ref mCommunicationData::mState is either
 *       @li @ref ConnectionPending::NotConnected (if @ref mCommunicationData::mAbortFlag is TRUE)
 *           or
 *       @li @ref ConnectionPending::ConnectionPending (otherwise, including on error).
 *
 * @param error If an error is encountered, this function returns FALSE and sets this value
 *        accordingly.
 *
 * Internal function that starts the connect. All connection parameters (hostname,
 * port, state, ...) must have been set already.
 *
 * This function sets up the socket and connects to the remote host. Once connected, this function
 * returns success - no VNC communication is performed by this function.
 *
 * @return TRUE on success (connection initiated), FALSE if mAbortFlag is TRUE or on error.
 *         If this function returns FALSE, @p error is set accordingly.
 **/
// TODO: maybe use orv_error_t as return value instead?
//       -> this way we cannot forget to set an error.
bool ConnectionThread::startConnection(orv_error_t* error)
{
    orv_error_reset(error);
    mCommunicationData->mMutex.lock();
    bool abort = mCommunicationData->mAbortFlag;
    mPort = mCommunicationData->mPort;
    strncpy(mHostName, mCommunicationData->mHostName, ORV_MAX_HOSTNAME_LEN);
    mHostName[ORV_MAX_HOSTNAME_LEN] = '\0';
    mPasswordLength = mCommunicationData->mPasswordLength;
    mCommunicationData->mPasswordLength = 0;
    mPassword = mCommunicationData->mPassword; // NOTE: we take ownership!
    mCommunicationData->mPassword = nullptr;
    orv_vnc_server_capabilities_reset(&mServerCapabilities);
    orv_vnc_server_capabilities_reset(&mCommunicationData->mServerCapabilities);
    mConnectionInfo.reset();
    mCommunicationData->mConnectionInfo.reset();
    if (abort) {
        changeStateMutexLocked(ConnectionState::NotConnected);
        orv_error_set(error, ORV_ERR_USER_INTERRUPTION, 0, ORV_ERROR_MSG_USER_INTERRUPTION);
    }
    else if (mCommunicationData->mState != ConnectionState::StartConnection) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Internal error: Invalid connection state %d when attempting to start a connection", (int)mCommunicationData->mState);
        abort = true;
    }
    if (!abort) {
        changeStateMutexLocked(ConnectionState::ConnectionPending);
    }
    mCommunicationData->mMutex.unlock();

    if (abort) {
        ORV_DEBUG(mContext, "Exiting connection immediately, no connection is being established.");
        if (!error->mHasError) {
            orv_error_set(error, ORV_ERR_USER_INTERRUPTION, 0, ORV_ERROR_MSG_USER_INTERRUPTION);
        }
        return false;
    }
    if (mPort == 0) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Invalid parameters: port==0 is not allowed");
        return false;
    }
    if (mHostName[0] == '\0') {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Invalid parameters: Empty hostname is not allowed");
        return false;
    }
    if (mSocket.isOpened()) {
        orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Internal error: No connection active, but socket is open");
        return false;
    }

    ORV_DEBUG(mContext, "Starting connection to %s:%d", mHostName, (int)mPort);

    mSocket.setSocketTimeoutSeconds(ORV_SOCKET_TIMEOUT_SECONDS);

    if (!mSocket.makeSocketAndConnectBlockingTo(mHostName, mPort, error)) {
        if (!error->mHasError) {
            orv_error_set(error, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Internal error: Connect failed, but have no detailed error message");
        }
        return false;
    }

    mMessageFramebufferUpdate.resetConnection();

    return true;
}

/**
 * @pre No FramebufferUpdateRequest messages are currently pending to the server (otherwise the
 *      format of the next FramebufferUpdate messages are undefined - either the new or the old
 *      pixel format)
 *
 * Send a @ref ClientMessage::SetPixelFormat message to the server with the given format.
 * If sending this message fails, @p error will be set accordingly and this function returns FALSE,
 * otherwise the @p error will be reset and this function returns TRUE.
 *
 * After successfully sending the new pixel format to the server, all messages from the server are
 * expected to use the new pixel format. The @p format contents are tracked by this function in this
 * object.
 *
 * This function stores the @p format in @ref mCurrentPixelFormat AND updates the corresponding
 * value in @ref CommunicationData.
 *
 * @return Whether sending the message was successful. If this function returns TRUE, @p error is
 *         set accordingly, otherwise @p error is reset.
 *         This is equivalent to !(error->mHasError) of @p error.
 *         NOTE: Success does not indicate whether the server accepted the message in any way.
 **/
bool ConnectionThread::sendSetPixelFormat(orv_error_t* error, const orv_communication_pixel_format_t& format)
{
    ORV_DEBUG(mContext, "Sending SetPixelFormat to server");
    ORV_DEBUG(mContext, "  TrueColor: %s, BitsPerPixel: %d, depth: %d, max r/g/b: %d/%d/%d, r/g/b shift: %d/%d/%d, BigEndian: %s",
            format.mTrueColor ? "true" : "false",
            (int)format.mBitsPerPixel,
            (int)format.mDepth,
            (int)format.mColorMax[0],
            (int)format.mColorMax[1],
            (int)format.mColorMax[2],
            (int)format.mColorShift[0],
            (int)format.mColorShift[1],
            (int)format.mColorShift[2],
            format.mBigEndian ? "true" : "false");
    static const size_t bufferSize = 20;
    char buffer[bufferSize];
    Writer::writeUInt8(buffer + 0, (uint8_t)ClientMessage::SetPixelFormat);
    Writer::writeUInt8(buffer + 1, 0);
    Writer::writeUInt8(buffer + 2, 0);
    Writer::writeUInt8(buffer + 3, 0);
    writePixelFormat(buffer + 4, 16, format);
    if (!mSocket.writeDataBlocking(buffer, bufferSize, error)) {
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return false;
        }
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Failed to send SetPixelFormat message to server, attempted to write %d bytes", (int)bufferSize);
        return false;
    }
    orv_error_reset(error);
    mCurrentPixelFormat = format;
    std::unique_lock<std::mutex> lock(mCommunicationData->mMutex);
    mCommunicationData->mCommunicationPixelFormat = mCurrentPixelFormat;
    return true;
}

void ConnectionThread::sendSetEncodings(orv_error_t* error)
{
    ORV_DEBUG(mContext, "Sending SetEncodings to server");
    // NOTE: Order of encodings is a hint to the server defining the preference of encodings
    //       (first is most preferred). The server can ignore this hint.
    // NOTE: The "encodings" list also includes pseudo-encodings, which simply announce supported
    //       features to the server.
    // NOTE: atm we hardcode this list, including the order of encodings.
    static const int32_t supportedEncodings[] = {
        //(int32_t)EncodingType::ZRLE,
        (int32_t)EncodingType::Cursor, // pseudo-encoding
        (int32_t)EncodingType::CopyRect,
        (int32_t)EncodingType::zlib,
        (int32_t)EncodingType::Hextile,

        // WARNING: RRE/CoRRE in at least one released version of "WinVNC" (should be the realvnc
        //          server) is broken!
        //          Occasionally sends out wrong x values for rectangles (far outside the
        //          framebuffer, probably some invalid memory for one byte).
        //          Avoid these if possible!
        //          TODO: probably disable completely.
        (int32_t)EncodingType::CoRRE,
        (int32_t)EncodingType::RRE,

        (int32_t)EncodingType::Raw,
    };
    static const uint16_t numberOfEncodings = sizeof(supportedEncodings) / sizeof(int32_t);
    static const size_t bufferSize = 4 + 4 * numberOfEncodings;
    char buffer[bufferSize];
    Writer::writeUInt8(buffer + 0, (uint8_t)ClientMessage::SetEncodings);
    Writer::writeUInt8(buffer + 1, 0);
    Writer::writeUInt16(buffer + 2, numberOfEncodings);
    for (uint16_t i = 0; i < numberOfEncodings; i++) {
        Writer::writeInt32(buffer + 4 + 4*i, supportedEncodings[i]);
    }
    if (!mSocket.writeDataBlocking(buffer, bufferSize, error)) {
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return;
        }
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Failed to send SetEncodings message to server, attempted to write %d bytes", (int)bufferSize);
        return;
    }
    orv_error_reset(error);
}

bool ConnectionThread::sendFramebufferUpdateRequest(orv_error_t* error, bool incremental, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    ORV_DEBUG(mContext, "Sending FramebufferUpdateRequest message to server for x=%d, y=%d, size=%dx%d, incremental=%s", (int)x, (int)y, (int)w, (int)h, incremental?"true":"false");
    static const size_t bufferSize = 10;
    char buffer[bufferSize];
    Writer::writeUInt8(buffer + 0, (uint8_t)ClientMessage::FramebufferUpdateRequest);
    Writer::writeUInt8(buffer + 1, incremental ? 1 : 0);
    Writer::writeUInt16(buffer + 2, x);
    Writer::writeUInt16(buffer + 4, y);
    Writer::writeUInt16(buffer + 6, w);
    Writer::writeUInt16(buffer + 8, h);
    if (!mSocket.writeDataBlocking(buffer, bufferSize, error)) {
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return false;
        }
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Failed to send FramebufferUpdateRequest message to server, attempted to write %d bytes", (int)bufferSize);
        return false;
    }
    orv_error_reset(error);
    return true;
}

void ConnectionThread::sendKeyEvent(orv_error_t* error, bool down, uint32_t key)
{
    static const size_t bufferSize = 8;
    char buffer[bufferSize];
    Writer::writeUInt8(buffer + 0, (uint8_t)ClientMessage::KeyEvent);
    Writer::writeUInt8(buffer + 1, down ? 1 : 0);
    Writer::writeUInt8(buffer + 2, 0);
    Writer::writeUInt8(buffer + 3, 0);
    Writer::writeUInt32(buffer + 4, key);
    if (!mSocket.writeDataBlocking(buffer, bufferSize, error)) {
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return;
        }
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Failed to send KeyEvent message to server, attempted to write %d bytes", (int)bufferSize);
        return;
    }
    orv_error_reset(error);
}

void ConnectionThread::sendPointerEvent(orv_error_t* error, uint16_t x, uint16_t y, uint8_t buttonMask)
{
    //ORV_DEBUG(mContext, "Sending pointer event to server, x=%d, y=%d, buttonMask=%d", (int)x, (int)y, (int)buttonMask);
    static const size_t bufferSize = 6;
    char buffer[bufferSize];
    Writer::writeUInt8(buffer + 0, (uint8_t)ClientMessage::PointerEvent);
    Writer::writeUInt8(buffer + 1, buttonMask);
    Writer::writeUInt16(buffer + 2, x);
    Writer::writeUInt16(buffer + 4, y);
    if (!mSocket.writeDataBlocking(buffer, bufferSize, error)) {
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return;
        }
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Failed to send PointerEvent message to server, attempted to write %d bytes", (int)bufferSize);
        return;
    }
    orv_error_reset(error);
}

/**
 * @param text ISO 8859-1 (Latin-1) encoded text. Non-Latin1 text is currently not supported by this
 *        message type in the VNC protocol.
 **/
void ConnectionThread::sendClientCutText(orv_error_t* error, const char* text, uint32_t textLen)
{
    ORV_DEBUG(mContext, "Sending ClientCutText to server");
    if (textLen > 20 * 1024 * 1024) {
        ORV_ERROR(mContext, "Got request to send %u bytes as ClientCutText, which exceeds allowed size. Refusing to send data.", (unsigned int)textLen);
        return;
    }
    const size_t bufferSize = 8 + textLen;
    char* buffer = (char*)malloc(bufferSize);
    Writer::writeUInt8(buffer + 0, (uint8_t)ClientMessage::KeyEvent);
    Writer::writeUInt8(buffer + 1, 0);
    Writer::writeUInt8(buffer + 2, 0);
    Writer::writeUInt8(buffer + 3, 0);
    Writer::writeUInt32(buffer + 4, textLen);
    memcpy(buffer + 8, text, textLen);
    bool ok = mSocket.writeDataBlocking(buffer, bufferSize, error);
    free(buffer);
    buffer = nullptr;
    if (!ok) {
        if (error->mErrorCode == ORV_ERR_USER_INTERRUPTION) {
            return;
        }
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Failed to send ClientCutText message to server, attempted to write %u bytes", (unsigned int)bufferSize);
        return;
    }
    orv_error_reset(error);
}

/**
 * @pre The connection state is @ref ConnectionState::Connected
 * @pre The mutex is NOT locked
 * @post The connection state is @ref ConnectionState::NotConnected
 * @post The mUserRequestedDisconnect flag is not set anymore
 * @post The mAbortFlag flag is set if (and only if) mWantQuitThread is set
 *
 * Close the socket and send a "disconnected" event, indicating a broken connection with the
 * specified error. This function sets the current state of the connection to @ref
 * ConnectionState::NotConnected.
 **/
void ConnectionThread::disconnectWithError(const orv_error_t& error)
{
    closeSocket();
    std::unique_lock<std::mutex> lock(mCommunicationData->mMutex);
    free(mConnectionInfo.mDesktopName);
    mConnectionInfo.mDesktopName = nullptr;
    free(mCommunicationData->mConnectionInfo.mDesktopName);
    mCommunicationData->mConnectionInfo.mDesktopName = nullptr;
    changeStateMutexLocked(ConnectionState::NotConnected);
    mCommunicationData->mAbortFlag = mCommunicationData->mWantQuitThread;
    mCommunicationData->mUserRequestedDisconnect = false;
    ORV_DEBUG(mContext, "Disconnected due to error. Received bytes: %u, sent bytes: %u", (unsigned int)mSocket.receivedBytes(), (unsigned int)mSocket.sentBytes());
    lock.unlock();
    const uint8_t gracefulExit = 0;
    orv_event_t* event = orv_event_disconnected_init(mHostName, mPort, gracefulExit, &error);
    sendEvent(event);
}

/**
 * @pre The connection state is @ref ConnectionState::ConnectionPending
 * @pre The mutex is NOT locked
 * @post The connection state is @ref ConnectionState::Error
 * @post The mUserRequestedDisconnect flag is not set anymore
 * @post The mAbortFlag flag is set if (and only if) mWantQuitThread is set
 *
 * Similar to @ref disconnectWithError(), but for not-yet fully established connections.
 *
 * This function closes the socket and sends a "connection failed" event with the specified error.
 * This function sets the current state of the connection to @ref ConnectionState::Error.
 **/
void ConnectionThread::abortConnectWithError(const orv_error_t& error, orv_auth_type_t authType)
{
    closeSocket();
    std::unique_lock<std::mutex> lock(mCommunicationData->mMutex);
    changeStateMutexLocked(ConnectionState::NotConnected);
    mCommunicationData->mAbortFlag = mCommunicationData->mWantQuitThread;
    mCommunicationData->mUserRequestedDisconnect = false;
    lock.unlock();
    orv_event_t* event = orv_event_connect_result_init(mHostName, mPort, 0, 0, nullptr, nullptr, authType, &error);
    sendEvent(event);

    const uint8_t gracefulExit = 0;
    orv_event_t* disconnectedEvent = orv_event_disconnected_init(mHostName, mPort, gracefulExit, &error);
    sendEvent(disconnectedEvent);
}

/**
 * Clear the password from memory, if any is set. The password can no longer be used after this
 * function has been called.
 **/
void ConnectionThread::clearPassword()
{
    if (mPassword) {
        memset(mPassword, 0, mPasswordLength);
        mPasswordLength = 0;
        free(mPassword);
        mPassword = nullptr;
    }
}

char* ConnectionThread::allocateThreadNameString() const
{
    // NOTE: must be a short name, e.g. pthread_setname_np() limits to 16 characters including \0
    std::stringstream ss;
    ss << std::this_thread::get_id();
    const std::string s = ss.str();
    size_t len = s.size();
    char* ret = (char*)malloc(len + 1);
    strncpy(ret, s.c_str(), len);
    ret[len] = '\0';
    return ret;
}

/**
 * @pre The mutex of @ref mCommunicationData is LOCKED
 *
 * Helper function used for state changes in the connection thread.
 *
 * NOTE: State changes from @ref ConnectionState::NotConnected or from @ref ConnectionState::Error
 *       to another state (most notable to @ref ConnectionState::StartConnection) may be performed
 *       by the @ref OrvVncClient directly (bypassing this function).
 *       However all changes away from @ref ConnectionState::Connected are required to use this
 *       function, to ensure internal member variables are cleaned up, if required.
 **/
void ConnectionThread::changeStateMutexLocked(ConnectionState state)
{
    if (mCommunicationData->mState == ConnectionState::Connected) {
        mReceiveBufferOffset = 0;
    }
    if (state == ConnectionState::Connected) {
        mReceiveBufferOffset = 0;
    }
    else {
        orv_vnc_server_capabilities_reset(&mCommunicationData->mServerCapabilities);
        orv_communication_pixel_format_reset(&mCommunicationData->mCommunicationPixelFormat);
    }
    mCommunicationData->mState = state;
    mCommunicationData->mClientSendEvents.clear();
}

/**
 * @pre The @ref CommunicationData::mMutex is LOCKED
 *
 * Allocate the framebuffer array for the current framebuffer size in @ref
 * CommunicationData::mFramebuffer.
 *
 * If the size specified in @ref CommunicationData::mFramebuffer exceeds the valid size, no
 * framebuffer is allocated and @p error is set accordingly. Otherwise @p error is simply reset.
 *
 * Note: This allocates the @em internal framebuffer with the @em internal pixel format, which does
 * not have to match the pixel format used in the communication with the server.
 **/
void ConnectionThread::allocateFramebufferMutexLocked(orv_error_t* error)
{
    orv_error_reset(error);
    if (!checkFramebufferSize(mCommunicationData->mFramebuffer.mWidth, mCommunicationData->mFramebuffer.mHeight, mCommunicationData->mFramebuffer.mBitsPerPixel, error)) {
        return;
    }
    size_t size = (size_t)mCommunicationData->mFramebuffer.mWidth * (size_t)mCommunicationData->mFramebuffer.mHeight * (size_t)mCommunicationData->mFramebuffer.mBytesPerPixel;
    if (size == 0) {
        size = 1;
    }
    free(mCommunicationData->mFramebuffer.mFramebuffer);
    mCommunicationData->mFramebuffer.mFramebuffer = nullptr;
    mCommunicationData->mFramebuffer.mFramebuffer = (uint8_t*)calloc(size, 1);
    mCommunicationData->mFramebuffer.mSize = size;
}

/**
 * Check if the provided framebuffer size parameters are valid.
 * This function checks if
 * - the @p framebufferWidth does not exceed ORV_MAX_FRAMEBUFFER_WIDTH
 * - the @p framebufferHeight does not exceed ORV_MAX_FRAMEBUFFER_HEIGHT
 * - the total framebuffer array size (width times height times bytesPerPixel, which is calculated
 *   from @p bitsPerPixel) does not exceed @p ORV_MAX_FRAMEBUFFER_MEMORY.
 *
 * If the parameters are valid, @p error is simply reset, otherwise @p error is set to an error
 * value.
 *
 * Note that @p bitsPerPixel does @em not have to be a valid value according to the RFB protocol. In
 * particular, 24 bits per pixel (simple RGB 888) is accepted by this function.
 *
 * @return TRUE if the parameters are usable, otherwise FALSE. This matches the @ref
 *         orv_error_t::mHasError flag of @p error.
 **/
bool ConnectionThread::checkFramebufferSize(uint16_t framebufferWidth, uint16_t framebufferHeight, uint8_t bitsPerPixel, orv_error_t* error)
{
    orv_error_reset(error);
    static_assert(((uint64_t)std::numeric_limits<size_t>::max()) >= ((uint64_t)(ORV_MAX_FRAMEBUFFER_MEMORY)),
            "Size of ORV_MAX_FRAMEBUFFER_MEMORY exceeds maximum value of size_t on this platform");
    uint8_t bytesPerPixel = (uint8_t)(((uint16_t)bitsPerPixel + 7) / 8);
    uint64_t requiredMemory = framebufferWidth * framebufferHeight * bytesPerPixel; // uint16_t*uint16_t*uint8_t requires up to 40 bits, i.e. always fits into a uint64_t
    if (framebufferWidth == 0) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Framebuffer width of 0 is invalid and unusable.");
        return false;
    }
    if (framebufferHeight == 0) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Framebuffer height of 0 is invalid and unusable.");
        return false;
    }
    // NOTE: range checks for framebuffer width/height are noops if max width/height are 64K, as
    //       that is the maximum value for uint16_t anyway.
    //       we perform them here anyway, so we can easily reduce the max values.
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wtautological-constant-out-of-range-compare"
#endif // __GNUC__
    if (framebufferWidth > ORV_MAX_FRAMEBUFFER_WIDTH) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Framebuffer width %d exceeds the maximum width of %d allowed by this library.", (int)framebufferWidth, (int)ORV_MAX_FRAMEBUFFER_WIDTH);
        return false;
    }
    if (framebufferHeight > ORV_MAX_FRAMEBUFFER_HEIGHT) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Framebuffer height %d exceeds the maximum height of %d allowed by this library.", (int)framebufferHeight, (int)ORV_MAX_FRAMEBUFFER_HEIGHT);
        return false;
    }
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif // __GNUC__
    if (requiredMemory > (uint64_t)ORV_MAX_FRAMEBUFFER_MEMORY) {
        orv_error_set(error, ORV_ERR_GENERIC, 0, "Framebuffer size %dx%d at %d bits per pixel required %" PRIu64 " bytes of memory. This library allows at most a framebuffer of size %" PRIu64 ".", (int)framebufferWidth, (int)framebufferHeight, (int)bitsPerPixel, (uint64_t)requiredMemory, (uint64_t)ORV_MAX_FRAMEBUFFER_MEMORY);
        return false;
    }
    return true;
}

} // namespace vnc
} // namespace openrv

