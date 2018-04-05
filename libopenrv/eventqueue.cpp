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

