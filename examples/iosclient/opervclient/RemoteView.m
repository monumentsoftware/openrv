//
//  RemoteView.m
//  opervclient
//
//  Created by christoph on 03.04.18.
//  Copyright © 2018 Christoph Möde. All rights reserved.
//

#import "RemoteView.h"
#import "FramebufferLayer.h"

@implementation RemoteView
{
    FramebufferLayer* mFBLayer;
}
-(instancetype) initWithFrame:(CGRect)frame
{
    self = [super initWithFrame:frame];
    if (self) {
        mFBLayer = [[FramebufferLayer alloc] initWithFrame:frame];
        [self.layer addSublayer:mFBLayer];
    }
    return self;
}


@end
