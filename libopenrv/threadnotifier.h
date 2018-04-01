#ifndef OPENRV_THREADNOTIFIER_H
#define OPENRV_THREADNOTIFIER_H

#include <mutex>

namespace openrv {

/**
 * Wrapper class for the write-end of a local pipe. This object provides a @ref sendNotification()
 * method that can be used to wake up the thread that listens on the read-end of the pipe, so that
 * the thread can do something.
 **/
class ThreadNotifierWriter
{
public:
    ThreadNotifierWriter();
    virtual ~ThreadNotifierWriter();

    void close();
    void sendNotification();

    bool isValid() const;
#ifndef _MSC_VER
    void setWriteFd(int pipeWriteFd);
    int pipeWriteFd() const;
#else // _MSC_VER
    void setWriteHandle(void* pipeWriteHandle);
    void* pipeWriteHandle() const;
#endif // _MSC_VER

private:
#ifndef _MSC_VER
    int mPipeWriteFd = -1;
#else // _MSC_VER
    void* mPipeWriteHandle = nullptr; // actually a HANDLE
#endif // _MSC_VER
};

/**
 * Wrapper class for the read-end of a local pipe. This object is meant to get notified by a
 * different thread (through the pipe) when some event occurs that should trigger the thread to do
 * something.
 **/
class ThreadNotifierListener
{
public:
    ThreadNotifierListener();
    virtual ~ThreadNotifierListener();

    void close();
    void swallowPipeData();

    bool isValid() const;
#ifndef _MSC_VER
    void setReadFd(int pipeReadFd);
    int pipeReadFd() const;
#else // _MSC_VER
    void setReadHandle(void* pipeReadHandle);
    void* pipeReadHandle() const;
#endif // _MSC_VER

private:
#ifndef _MSC_VER
    int mPipeReadFd = -1;
#else // _MSC_VER
    void* mPipeReadHandle = nullptr; // actually a HANDLE
#endif // _MSC_VER
};

/**
 * Helper class that takes a @ref ThreadNotifierWriter and a @ref ThreadNotifierListener and turns
 * them into a pipe.
 **/
class ThreadNotifier
{
public:
    static bool makePipe(ThreadNotifierWriter* writer, ThreadNotifierListener* listener);
};

/**
 * @return TRUE if the handle of this object is valid, otherwise FALSE (handle could not be created properly).
 **/
inline bool ThreadNotifierWriter::isValid() const
{
#ifndef _MSC_VER
    if (mPipeWriteFd == -1) {
        return false;
    }
#else // _MSC_VER
    if (mPipeWriteHandle == nullptr) {
        return false;
    }
#endif // _MSC_VER
    return true;
}

#ifndef _MSC_VER
inline int ThreadNotifierWriter::pipeWriteFd() const
{
    return mPipeWriteFd;
}
#else // _MSC_VER
inline void* ThreadNotifierWriter::pipeWriteHandle() const
{
    return (void*)mPipeWriteHandle;
}

#endif // _MSC_VER


/**
* @return TRUE if the handle of this object is valid, otherwise FALSE (handle could not be created properly).
**/
inline bool ThreadNotifierListener::isValid() const
{
#ifndef _MSC_VER
    if (mPipeReadFd == -1) {
        return false;
    }
#else // _MSC_VER
    if (mPipeReadHandle == nullptr) {
        return false;
    }
#endif // _MSC_VER
    return true;
}

#ifndef _MSC_VER
inline int ThreadNotifierListener::pipeReadFd() const
{
    return mPipeReadFd;
}
#else // _MSC_VER
inline void* ThreadNotifierListener::pipeReadHandle() const
{
    return (void*)mPipeReadHandle;
}

#endif // _MSC_VER

} // namespace openrv

#endif

