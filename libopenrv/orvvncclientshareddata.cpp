#include "orvvncclientshareddata.h"
#include <libopenrv/libopenrv.h>

#include <string.h>

namespace openrv {
namespace vnc {

void ConnectionInfo::copy(ConnectionInfo* dst)
{
    free(dst->mDesktopName);
    *dst = *this;
    dst->mDesktopName = strdup(mDesktopName);
}


OrvVncClientSharedData::OrvVncClientSharedData()
{
    orv_communication_pixel_format_reset(&mRequestFormat);
    memset(&mFramebuffer, 0, sizeof(orv_framebuffer_t));
    memset(&mCursorData, 0, sizeof(orv_cursor_t));
    orv_vnc_server_capabilities_reset(&mServerCapabilities);
    orv_communication_pixel_format_reset(&mConnectionInfo.mDefaultPixelFormat);
}
OrvVncClientSharedData::~OrvVncClientSharedData()
{
    free(mFramebuffer.mFramebuffer);
    free(mCursorData.mCursor);
}

void OrvVncClientSharedData::clearPasswordMutexLocked()
{
    if (mPassword) {
        memset(mPassword, 0, mPasswordLength);
        mPasswordLength = 0;
        free(mPassword);
        mPassword = nullptr;
    }
}

bool operator==(const RequestFramebuffer& f1, const RequestFramebuffer& f2)
{
    if (f1.mIncremental == f2.mIncremental &&
        f1.mX == f2.mX &&
        f1.mY == f2.mY &&
        f1.mW == f2.mW &&
        f1.mH == f2.mH) {
        return true;
    }
    return false;
}

bool operator!=(const RequestFramebuffer& f1, const RequestFramebuffer& f2)
{
    return !(f1 == f2);
}


} // namespace vnc
} // namespace openrv

