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
