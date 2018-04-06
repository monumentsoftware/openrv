//
//  FramebufferLayer.m
//  opervclient
//
//  Created by christoph on 03.04.18.
//  Copyright © 2018 Christoph Möde. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "FramebufferLayer.h"

@implementation FramebufferLayer
{
    CGContextRef mContext;
}
-(instancetype) initWithFrame:(CGRect)frame
{
    self = [super init];
    if (self) {
        self.opaque = YES;
        [super setFrame:frame];
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        mContext = CGBitmapContextCreate(NULL, (size_t)frame.size.width, (size_t)frame.size.height, 8,
                   (size_t)frame.size.width * 4, colorSpace, kCGImageAlphaNoneSkipFirst);
        CGColorSpaceRelease(colorSpace);
    }
    return self;
}
-(void) dealloc
{
    CGContextRelease(mContext);
}
-(void) render
{
    CGImageRef img = CGBitmapContextCreateImage(mContext);
    self.contents = (__bridge id)img;
    CGImageRelease(img);
}
-(uint8_t*) framebuffer
{
    return CGBitmapContextGetData(mContext);
}
@end
