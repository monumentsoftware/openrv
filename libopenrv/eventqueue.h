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

