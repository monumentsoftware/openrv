#import "OpenRV.h"


static void orvEventCallback(struct orv_context_t* ctx, orv_event_t* event);


@implementation OpenRVContext
{
    orv_context_t* mContext;
    const orv_framebuffer_t* mFramebuffer;
}
-(instancetype) init
{
    self = [super init];
    if (self) {
        self.port = 9999;
        self.viewOnly = YES;
        self.framebufferSize = CGSizeMake(0, 0);
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
-(void) connectToHost:(NSString*)host password:(NSString*)password
{
    orv_connect_options_t conOptions;
    orv_connect_options_default(&conOptions);
    conOptions.mViewOnly = self.viewOnly ? false:true;
    conOptions.mCommunicationQualityProfile = ORV_COMM_QUALITY_PROFILE_BEST; // fixme
    orv_error_t err;
    int res = orv_connect(mContext, host.UTF8String, self.port, password.UTF8String, &conOptions, &err);
}
-(void) disconnect
{
    orv_disconnect(mContext);
}
-(BOOL) lockFramebuffer
{
    if (mFramebuffer) {
        return YES;
    }
    mFramebuffer = orv_acquire_framebuffer(mContext);
    if (!mFramebuffer) {
        return NO;
    }
    self.framebufferSize = CGSizeMake(mFramebuffer->mWidth, mFramebuffer->mHeight);
    return YES;
}
-(void) unlockFramebuffer
{
    if (!mFramebuffer) {
        return;
    }
    orv_release_framebuffer(mContext);
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
            if ([delegate respondsToSelector:@selector(contextDidConnect:withError:)]) {
                [delegate contextDidConnect:self withError:&connectResult->mError];
            }
        }
        break;
    case ORV_EVENT_DISCONNECTED:
        {
            if ([delegate respondsToSelector:@selector(contextDidDisconnected:)]) {
                [delegate contextDidDisconnected:self];
            }
        }
        break;
    case ORV_EVENT_FRAMEBUFFER_UPDATED:
        {
            orv_event_framebuffer_t* fb = event->mEventData;
            CGRect rect = CGRectMake(fb->mX, fb->mY, fb->mWidth, fb->mHeight);
            if ([delegate respondsToSelector:@selector(contextFramebufferUpdated:frame:)]) {
                [delegate contextFramebufferUpdated:self frame:rect];
            }
        }
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
