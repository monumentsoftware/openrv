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


@protocol OpenRVContextDelegate<NSObject>
@optional
    -(void) contextDidConnect:(OpenRVContext*)context withError:(const orv_error_t*)error;
    -(void) contextDidDisconnected:(OpenRVContext*)context;
    -(void) contextFramebufferUpdated:(OpenRVContext*)context frame:(CGRect)frame;
@end

@interface OpenRVContext : NSObject
-(instancetype) init;
-(void) connectToHost:(NSString*)host password:(NSString*)password;
-(void) disconnect;
-(BOOL) lockFramebuffer;
-(void) unlockFramebuffer;

@property(nonatomic, assign) uint16_t port;
@property(nonatomic, assign) BOOL viewOnly;
@property(nonatomic, assign) CGSize framebufferSize;
@property(nullable,nonatomic,weak) id<OpenRVContextDelegate> delegate;
@end
