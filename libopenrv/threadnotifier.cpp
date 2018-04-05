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

#include "threadnotifier.h"

#if !defined(_MSC_VER)
#include <unistd.h>
#include <sys/socket.h>
#else // _MSC_VER
#include <Windows.h>
#endif // _MSC_VER

namespace openrv {

ThreadNotifierWriter::ThreadNotifierWriter()
{
}

ThreadNotifierWriter::~ThreadNotifierWriter()
{
    close();
}

#ifndef _MSC_VER
/**
 * @param pipeWriteFd The write-end of the local pipe. This object takes ownership of the fd
 *        and closes it on destruction.
 **/
void ThreadNotifierWriter::setWriteFd(int pipeWriteFd)
{
    close();
    mPipeWriteFd = pipeWriteFd;
}
#else // _MSC_VER
/**
 * @param pipeWriteHandle The write-end of the local pipe. This object takes ownership of the handle
 *        and closes it on destruction.
 *        On windows this is a HANDLE.
 **/
void ThreadNotifierWriter::setWriteHandle(void* pipeWriteHandle)
{
    close();
    mPipeWriteHandle = pipeWriteHandle;
}
#endif // _MSC_VER

void ThreadNotifierWriter::close()
{
#ifndef _MSC_VER
    if (mPipeWriteFd != -1) {
        ::close(mPipeWriteFd);
        mPipeWriteFd = -1;
    }
#else // _MSC_VER
    if (mPipeWriteHandle != 0) {
        CloseHandle((HANDLE)mPipeWriteHandle);
        mPipeWriteHandle = 0;
    }
#endif // _MSC_VER
}

/**
 * Write a byte to the local pipe, signalling the other thread to wake up.
 **/
void ThreadNotifierWriter::sendNotification()
{
#ifndef _MSC_VER
    if (mPipeWriteFd != -1) {
        char c = 1;
        ::write(mPipeWriteFd, &c, 1);
    }
#else // _MSC_VER
    if (mPipeWriteHandle != 0) {
        SetEvent((HANDLE)mPipeWriteHandle);
    }
#endif // _MSC_VER
}

ThreadNotifierListener::ThreadNotifierListener()
{
}

ThreadNotifierListener::~ThreadNotifierListener()
{
    close();
}

#ifndef _MSC_VER
/**
 * @param pipeReadFd The read-end of the local pipe. This object takes ownership of the fd and
 *        closes it on destruction.
 **/
void ThreadNotifierListener::setReadFd(int pipeReadFd)
{
    close();
    mPipeReadFd = pipeReadFd;
}
#else // _MSC_VER
/**
 * @param pipeReadHandle The read-end of the local pipe. This object takes ownership of the handle and
 *        closes it on destruction.
 *        On windows this is a HANDLE, otherwise a pipe fd (int).
 **/
void ThreadNotifierListener::setReadHandle(void* pipeReadHandle)
{
    close();
    mPipeReadHandle = pipeReadHandle;
}
#endif // _MSC_VER

void ThreadNotifierListener::close()
{
#ifndef _MSC_VER
    if (mPipeReadFd != -1) {
        ::close(mPipeReadFd);
        mPipeReadFd = -1;
    }
#else // _MSC_VER
    if (mPipeReadHandle != 0) {
        CloseHandle((HANDLE)mPipeReadHandle);
        mPipeReadHandle = 0;
    }
#endif // _MSC_VER
}

/**
 * @pre Called by connection thread
 *
 * Swallow data written to the local pipe, so that it can be used again to signal the thread.
 **/
void ThreadNotifierListener::swallowPipeData()
{
#ifndef _MSC_VER
    if (mPipeReadFd == -1) {
        return;
    }
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(mPipeReadFd, &readfds);
    const int nfds = mPipeReadFd + 1;
    bool moreData = true;
    while (moreData) {
        int ret = select(nfds, &readfds, nullptr, nullptr, &timeout);
        if (ret > 0) {
            char c;
            ::read(mPipeReadFd, &c, 1);
        }
        else {
            moreData = false;
        }
    }
#else // _MSC_VER
    if (mPipeReadHandle == nullptr) {
        return;
    }
    ResetEvent((HANDLE)mPipeReadHandle);
#endif // _MSC_VER
}


bool ThreadNotifier::makePipe(ThreadNotifierWriter* writer, ThreadNotifierListener* listener)
{
#ifndef _MSC_VER
    int pipeFds[2];
    if (pipe(pipeFds) != 0) {
        return false;
    }
    writer->setWriteFd(pipeFds[1]);
    listener->setReadFd(pipeFds[0]);
#else // _MSC_VER
    SECURITY_ATTRIBUTES* attr = nullptr;
    bool manualReset = true;
    bool initialState = false;
    LPCWSTR name = nullptr;
    HANDLE event = CreateEvent(attr, manualReset, initialState, name);
    if (!event) {
        return false;
    }
    writer->setWriteHandle(event);
    listener->setReadHandle(event);
    return true;
#endif // _MSC_VER
    return true;
}

} // namespace openrv

