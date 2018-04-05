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

#ifndef OPENRV_ORV_CONTEXT_H
#define OPENRV_ORV_CONTEXT_H

#include <libopenrv/libopenrv.h>

namespace openrv {

namespace vnc {
    class OrvVncClient;
}
class EventQueue;

} // namespace openrv

/**
 * @file orv_context.h
 *
 * Internal header defining the orv_context_t struct.
 *
 * This file is @em not public API.
 **/

struct orv_context_t
{
    orv_config_t mConfig;
    openrv::vnc::OrvVncClient* mClient = nullptr;

    /**
     * Event queue that is used when the "polling" event callback is used. Otherwise this remains
     * NULL.
     **/
    openrv::EventQueue* mEventQueue = nullptr;

    union UserDataUnion
    {
        void* mPointerData;
        int mIntData;
    };
    UserDataUnion mUserData[(int)ORV_USER_DATA_COUNT];
};

#endif
