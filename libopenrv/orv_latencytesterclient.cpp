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

#include <libopenrv/orv_latencytesterclient.h>
#include <libopenrv/orv_logging.h>
#include <libopenrv/orv_error.h>
#include <libopenrv/libopenrv.h>
#include "socket.h"
#include "threadnotifier.h"

#include <string.h>
#include <stdlib.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>

struct orv_latency_tester_client_t
{
    orv_latency_tester_client_t(orv_context_t* orvContext, const char* hostName, int port, orv_latency_tester_event_callback_t eventCallback, void* userData);
    ~orv_latency_tester_client_t();
    void threadFunction(std::unique_lock<std::mutex> lock);
    void doThread(std::unique_lock<std::mutex>& lock);

    orv_context_t* mOrvContext = nullptr;
    char* mHostName = nullptr;
    int mPort = 0;
    orv_latency_tester_event_callback_t mEventCallback = nullptr;
    void* mUserData = nullptr;
    bool mWantQuit = false;
    std::mutex mMutex;
    std::condition_variable mWaitCondition;
    std::thread* mThread = nullptr;
    int mSocketFd = 0;
    openrv::Socket* mSocket = nullptr;
    openrv::ThreadNotifierListener mThreadNotifierListener;
    openrv::ThreadNotifierWriter mThreadNotifierWriter;
    bool mIsValid = true;
    bool mWantSendRequest = false;
};

orv_latency_tester_client_t::orv_latency_tester_client_t(orv_context_t* orvContext, const char* hostName, int port, orv_latency_tester_event_callback_t eventCallback, void* userData)
    : mOrvContext(orvContext),
    mHostName(strdup(hostName)),
    mPort(port),
    mEventCallback(eventCallback),
    mUserData(userData)
{
    if (!openrv::ThreadNotifier::makePipe(&mThreadNotifierWriter, &mThreadNotifierListener)) {
        ORV_ERROR(mOrvContext, "Failed to create pipe");
    }
    mSocket = new openrv::Socket(orvContext, &mThreadNotifierListener, &mMutex, &mWantQuit);
    mThread = new std::thread(&orv_latency_tester_client_t::threadFunction, this, std::unique_lock<std::mutex>(mMutex));
}

orv_latency_tester_client_t::~orv_latency_tester_client_t()
{
    mMutex.lock();
    mWantQuit = true;
    mWaitCondition.notify_all();
    mMutex.unlock();

    mThreadNotifierWriter.sendNotification();

    mThread->join();
    delete mThread;
    delete mSocket;
    free(mHostName);
}

void orv_latency_tester_client_t::threadFunction(std::unique_lock<std::mutex> lock)
{
    doThread(lock);
    mIsValid = false;
    mSocket->close();

    orv_disconnected_t disconnected;
    memset(&disconnected, 0, sizeof(orv_disconnected_t));
    strncpy(disconnected.mHostName, mHostName, ORV_MAX_HOSTNAME_LEN);
    disconnected.mHostName[ORV_MAX_HOSTNAME_LEN] = '\0';
    disconnected.mPort = mPort;
    mEventCallback(this, ORV_LATENCY_TESTER_DISCONNECTED, &disconnected, mUserData);
}

void orv_latency_tester_client_t::doThread(std::unique_lock<std::mutex>& lock)
{
    if (mWantQuit) {
        return;
    }
    if (!mThreadNotifierListener.isValid()) {
        ORV_ERROR(mOrvContext, "Invalid ThreadNotifierListener, cannot connect to latency tester");
        return;
    }

    lock.unlock();

    orv_connect_result_t connectResult;
    memset(&connectResult, 0, sizeof(orv_connect_result_t));
    strncpy(connectResult.mHostName, mHostName, ORV_MAX_HOSTNAME_LEN);
    connectResult.mHostName[ORV_MAX_HOSTNAME_LEN] = '\0';
    connectResult.mPort = mPort;

    ORV_DEBUG(mOrvContext, "Connecting to latency tester at %s:%d...", mHostName, (int)mPort);
    if (!mSocket->makeSocketAndConnectBlockingTo(mHostName, mPort, &connectResult.mError)) {
        ORV_ERROR(mOrvContext, "Failed to connect to %s:%d, error: %s", mHostName, (int)mPort, connectResult.mError.mErrorMessage);
        mEventCallback(this, ORV_LATENCY_TESTER_CONNECT_RESULT, &connectResult, mUserData);
        return;
    }
    const size_t bufferSize = 1024;
    char buffer[bufferSize + 1] = {};
    if (!mSocket->readDataBlocking(buffer, strlen("latencytester"), &connectResult.mError)) {
        ORV_ERROR(mOrvContext, "Failed to read server HELO");
        mSocket->close();
        orv_error_set(&connectResult.mError, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Failed to read server HELO");
        mEventCallback(this, ORV_LATENCY_TESTER_CONNECT_RESULT, &connectResult, mUserData);
        return;
    }
    if (strcmp(buffer, "latencytester") != 0) {
        ORV_ERROR(mOrvContext, "Unexpected server HELO.");
        mSocket->close();
        orv_error_set(&connectResult.mError, ORV_ERR_CONNECT_ERROR_GENERIC, 0, "Unexpected server HELO");
        mEventCallback(this, ORV_LATENCY_TESTER_CONNECT_RESULT, &connectResult, mUserData);
        return;
    }
    ORV_DEBUG(mOrvContext, "Connected to %s:%d.", mHostName, (int)mPort);
    mEventCallback(this, ORV_LATENCY_TESTER_CONNECT_RESULT, &connectResult, mUserData);

    lock.lock();
    while (!mWantQuit) {
        if (mWantSendRequest) {
            uint8_t updateRequestMessage[5] = {};
            memcpy(updateRequestMessage, "lat", 3);
            updateRequestMessage[3] = 1;
            lock.unlock();
            orv_error_t error;
            struct timeval sendTimestamp;
            gettimeofday(&sendTimestamp, nullptr);
            if (!mSocket->writeDataBlocking(updateRequestMessage, 4, &error)) {
                ORV_ERROR(mOrvContext, "Failed to write update request, error: %s", error.mErrorMessage);
                mSocket->close();
                return;
            }
            const int updateResponseSize = 4 + (4*2) * 4 + (3*2) * 8;
            uint8_t updateResponse[updateResponseSize];
            if (!mSocket->readDataBlocking(updateResponse, updateResponseSize, &error)) {
                ORV_ERROR(mOrvContext, "Failed to read update response, error: %s", error.mErrorMessage);
                mSocket->close();
                return;
            }
            struct timeval receiveTimestamp;
            gettimeofday(&receiveTimestamp, nullptr);
            if (memcmp(updateResponse, "lat", 3) != 0) {
                ORV_ERROR(mOrvContext, "Magic cookie mismatch in response from server");
                mSocket->close();
                return;
            }
            if (updateResponse[3] != 1) {
                ORV_ERROR(mOrvContext, "Unexpected response type %d from server after update request", (int)updateResponse[3]);
                mSocket->close();
                return;
            }
            orv_latency_tester_update_response_t response;
            memcpy(&response.mPreviousTopLeftX, updateResponse + 4, 4);
            memcpy(&response.mPreviousTopLeftY, updateResponse + 8, 4);
            memcpy(&response.mPreviousBottomRightX, updateResponse + 12, 4);
            memcpy(&response.mPreviousBottomRightY, updateResponse + 16, 4);
            memcpy(&response.mNewTopLeftX, updateResponse + 20, 4);
            memcpy(&response.mNewTopLeftY, updateResponse + 24, 4);
            memcpy(&response.mNewBottomRightX, updateResponse + 28, 4);
            memcpy(&response.mNewBottomRightY, updateResponse + 32, 4);
            memcpy(&response.mServerUpdateTimestampSec, updateResponse + 36, 8);
            memcpy(&response.mServerUpdateTimestampUSec, updateResponse + 44, 8);
            memcpy(&response.mServerSendTimestampSec, updateResponse + 52, 8);
            memcpy(&response.mServerSendTimestampUSec, updateResponse + 60, 8);
            memcpy(&response.mServerRequestReceiveTimestampSec, updateResponse + 68, 8);
            memcpy(&response.mServerRequestReceiveTimestampUSec, updateResponse + 76, 8);
            response.mClientSendTimestampSec = sendTimestamp.tv_sec;
            response.mClientSendTimestampUSec = sendTimestamp.tv_usec;
            response.mClientReceiveTimestampSec = receiveTimestamp.tv_sec;
            response.mClientReceiveTimestampUSec = receiveTimestamp.tv_usec;

            mEventCallback(this, ORV_LATENCY_TESTER_UPDATE_RESPONSE, &response, mUserData);

            lock.lock();
            mWantSendRequest = false;
        }
        else {
            mWaitCondition.wait(lock);
        }
    }
    mSocket->close();
    ORV_DEBUG(mOrvContext, "Finished connection to %s:%d", mHostName, (int)mPort);
}


extern "C" {

/**
 * @param orvContext An OpenRV context used for log messages. The connection in the @p orvContext is
 *                   not used. The context has to exist, but does not have to be in any partiucular
 *                   state.
 *                   The pointer must remain valid for the lifetime of this connection.
 **/
struct orv_latency_tester_client_t* orv_latency_tester_connect(struct orv_context_t* orvContext, orv_latency_tester_event_callback_t callback, void* userData, const char* hostName, int port)
{
    if (!orvContext) {
        return nullptr;
    }
    if (!hostName || strlen(hostName) == 0 || port <= 0 || port > 65535) {
        return nullptr;
    }
    struct orv_latency_tester_client_t* client = new orv_latency_tester_client_t(orvContext, hostName, port, callback, userData);
    return client;
}

void orv_latency_tester_disconnect(struct orv_latency_tester_client_t* client)
{
    if (!client) {
        return;
    }
    delete client;
}

int orv_latency_tester_request_update(struct orv_latency_tester_client_t* client)
{
    if (!client) {
        return 1;
    }
    std::unique_lock<std::mutex> lock(client->mMutex);
    if (!client->mIsValid) {
        return 1;
    }
    client->mWantSendRequest = true;
    client->mWaitCondition.notify_all();
    return 0;
}


}
