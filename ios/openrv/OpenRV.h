#import <UIKit/UIKit.h>
#import "libopenrv.h"

//! Project version number for openrv.
FOUNDATION_EXPORT double openrvVersionNumber;

//! Project version string for openrv.
FOUNDATION_EXPORT const unsigned char openrvVersionString[];

#import <openrv/libopenrv.h>
#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

@class OpenRVContext;
@class OpenRVFramebufferUpdate;

@interface OpenRVFramebuffer : NSObject
@property (nonatomic, readonly) uint32_t width;
@property (nonatomic, readonly) uint32_t height;
@property (nonatomic, readonly) uint32_t bitsPerPixel;
@property (nonatomic, readonly) const uint8_t* buffer;
@end

@protocol OpenRVContextDelegate<NSObject>
@optional
    -(void) openRVContextDidConnect:(OpenRVContext*)context;
    -(void) openRVContextConnectFailed:(OpenRVContext*)context withError:(const orv_error_t*)error;
    -(void) openRVContextDidDisconnected:(OpenRVContext*)context;
    -(void) openRVContextFramebufferUpdated:(OpenRVContext*)context frame:(CGRect)frame;
    -(void) openRVContextFramebufferUpdateFinished:(OpenRVContext*)context;
@end

@interface OpenRVContext : NSObject
-(instancetype) init;
-(void) setCredentials:(nullable NSString*)username password:(nullable NSString*)password;
-(void) connectToHost:(nonnull NSString*)host;
-(void) disconnect;

// frame buffer access
-(OpenRVFramebuffer*) lockFramebuffer;
-(void) unlockFramebuffer;

-(void) sendKeyEvent:(uint32_t)keyCode isDown:(BOOL)down;
-(void) sendPointerEvent:(int32_t)x y:(int32_t)y buttonMask:(uint32_t)buttonMask;

@property(nonatomic, assign) uint16_t port;
@property(nonatomic, assign) BOOL viewOnly;
@property(nonatomic, assign) CGSize framebufferSize;
@property(nonatomic, assign) orv_auth_type_t authType;
@property(nullable,nonatomic,weak) id<OpenRVContextDelegate> delegate;
@end
