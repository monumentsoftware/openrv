#include "eventqueue.h"

namespace openrv {

EventQueue::EventQueue(orv_context_t* ctx)
    : mContext(ctx)
{
}

EventQueue::~EventQueue()
{
    while (!mQueue.empty()) {
        orv_event_destroy(mQueue.front());
        mQueue.pop();
    }
}

/**
 * Add @p event to this queue.
 *
 * This function is thread-safe.
 *
 * This function takes ownership of @p event and destroys it on destruction (unless ownership is
 * released using @ref dequeue first).
 **/
void EventQueue::queue(orv_event_t* event)
{
    std::unique_lock<std::mutex> lock(mMutex);
    mQueue.push(event);
}

/**
 * Obtain the next element in the queue (if any). Ownership of the returned element is transferred
 * to the caller, who is responsible to destroying it.
 *
 * This function is thread-safe.
 *
 * @return NULL if there is currently no event in the queue, otherwise the next element in the
 *         queue. Ownership of that element is transferred to the caller.
 **/
orv_event_t* EventQueue::dequeue()
{
    //ORV_DEBUG(mContext, "Dequeue");
    std::unique_lock<std::mutex> lock(mMutex);
    if (mQueue.empty()) {
        return nullptr;
    }
    orv_event_t* event = mQueue.front();
    mQueue.pop();
    return event;
}

} // namespace openrv

