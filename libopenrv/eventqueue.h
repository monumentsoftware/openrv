#ifndef OPENRV_EVENTQUEUE_H
#define OPENRV_EVENTQUEUE_H

#include <libopenrv/libopenrv.h>

#include <queue>
#include <mutex>

namespace openrv {

class EventQueue
{
public:
    explicit EventQueue(orv_context_t* ctx);
    virtual ~EventQueue();

    void queue(orv_event_t* event);
    orv_event_t* dequeue();

private:
    orv_context_t* mContext = nullptr;
    mutable std::mutex mMutex; // TODO: use lock-free queue instead?
    std::queue<orv_event_t*> mQueue;
};

} // namespace openrv

#endif

