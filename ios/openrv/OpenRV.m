#import "OpenRV.h"


static void orvEventCallback(struct orv_context_t* ctx, orv_event_t* event);

@implementation OpenRVFramebuffer
{
    const orv_framebuffer_t* mFramebuffer;
}
-(uint32_t) width
{
    return mFramebuffer->mWidth;
}
-(uint32_t) height
{
    return mFramebuffer->mHeight;
}
-(uint32_t) bitsPerPixel
{
    return mFramebuffer->mBitsPerPixel;
}
-(const uint8_t*) buffer
{
    return mFramebuffer->mFramebuffer;
}
-(void) setFramebuffer:(const orv_framebuffer_t*)fb
{
    mFramebuffer = fb;
}
@end

@implementation OpenRVContext
{
    orv_context_t* mContext;
    OpenRVFramebuffer* mFramebuffer;
}
-(instancetype) init
{
    self = [super init];
    if (self) {
        self.port = 38973;
        self.viewOnly = YES;
        self.framebufferSize = CGSizeMake(0, 0);
        mFramebuffer = [[OpenRVFramebuffer alloc] init];
        [self createContext];
    }
    return self;
}
-(void) dealloc
{
    orv_destroy(mContext);
}
-(void) createContext
{
    orv_config_t config;
    orv_config_zero(&config);
    config.mEventCallback = orvEventCallback;
    config.mUserData[0] = (__bridge void*)self;
    mContext = orv_init(&config);
}
-(void) setCredentials:(NSString *)username password:(NSString *)password
{
    int res = orv_set_credentials(mContext, username ? username.UTF8String:NULL, password ? password.UTF8String:NULL);
}
-(void) connectToHost:(NSString*)host
{
    orv_connect_options_t conOptions;
    orv_connect_options_default(&conOptions);
    conOptions.mViewOnly = self.viewOnly ? false:true;
    conOptions.mCommunicationQualityProfile = ORV_COMM_QUALITY_PROFILE_BEST; // fixme
    orv_error_t err;
    int res = orv_connect(mContext, host.UTF8String, self.port, &conOptions, &err);
}
-(void) disconnect
{
    orv_disconnect(mContext);
}
-(OpenRVFramebuffer*) lockFramebuffer
{
    const orv_framebuffer_t* fb = orv_acquire_framebuffer(mContext);
    if (!fb) {
        return nil;
    }
    self.framebufferSize = CGSizeMake(fb->mWidth, fb->mHeight);
    [mFramebuffer setFramebuffer:fb];
    return mFramebuffer;
}
-(void) unlockFramebuffer
{
    if (!mFramebuffer) {
        return;
    }
    orv_release_framebuffer(mContext);
}
-(void) sendKeyEvent:(uint32_t)keyCode isDown:(BOOL)down
{
    orv_send_key_event(mContext, down, keyCode);
}
-(void) sendPointerEvent:(int32_t)x y:(int32_t)y buttonMask:(uint32_t)buttonMask;
{
    orv_send_pointer_event(mContext, x, y, buttonMask);
}

-(void) handleEvent:(orv_event_t*)event
{
    id<OpenRVContextDelegate> delegate = self.delegate;
    switch (event->mEventType)
    {
    case ORV_EVENT_CONNECT_RESULT:
        {
            const orv_connect_result_t* connectResult = (const orv_connect_result_t*)event->mEventData;
            self.framebufferSize = CGSizeMake(connectResult->mFramebufferWidth, connectResult->mFramebufferHeight);
            self.authType = connectResult->mAuthenticationType;
            if (!connectResult->mError.mHasError) {
                if ([delegate respondsToSelector:@selector(openRVContextDidConnect:)]) {
                    [delegate openRVContextDidConnect:self];
                }
                orv_request_framebuffer_update(mContext, 0, 0, self.framebufferSize.width, self.framebufferSize.height);
            } else {
                if ([delegate respondsToSelector:@selector(openRVContextConnectFailed:withError:)]) {
                    [delegate openRVContextConnectFailed:self withError:&connectResult->mError];
                }
            }
        }
        break;
    case ORV_EVENT_DISCONNECTED:
        {
            if ([delegate respondsToSelector:@selector(openRVContextDidDisconnected:)]) {
                [delegate openRVContextDidDisconnected:self];
            }
        }
        break;
    case ORV_EVENT_FRAMEBUFFER_UPDATED:
        {
            orv_event_framebuffer_t* fb = event->mEventData;
            CGRect rect = CGRectMake(fb->mX, fb->mY, fb->mWidth, fb->mHeight);
            if ([delegate respondsToSelector:@selector(openRVContextFramebufferUpdated:frame:)]) {
                [delegate openRVContextFramebufferUpdated:self frame:rect];
            }
        }
        break;
    case ORV_EVENT_FRAMEBUFFER_UPDATE_REQUEST_FINISHED:
        {
            if ([delegate respondsToSelector:@selector(openRVContextFramebufferUpdateFinished:)]) {
                [delegate openRVContextFramebufferUpdateFinished:self];
            }
            orv_request_framebuffer_update(mContext, 0, 0, self.framebufferSize.width, self.framebufferSize.height);
        }
        break;
    case ORV_EVENT_NONE:
    case ORV_EVENT_CUT_TEXT:
    case ORV_EVENT_CURSOR_UPDATED:
    case ORV_EVENT_BELL:
    case ORV_EVENT_THREAD_STARTED:
    case ORV_EVENT_THREAD_ABOUT_TO_STOP:
        break;
    }
    orv_event_destroy(event);

}
@end



static void orvEventCallback(orv_context_t* ctx, orv_event_t* event)
{
    void* data = orv_get_user_data(ctx, ORV_USER_DATA_0);
    OpenRVContext* obj = (__bridge OpenRVContext*)data;
    dispatch_async(dispatch_get_main_queue(), ^{
        [obj handleEvent:event];
    });
}
