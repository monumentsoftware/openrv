//
//  RemoteView.h
//  opervclient
//
//  Created by christoph on 03.04.18.
//  Copyright © 2018 Christoph Möde. All rights reserved.
//


@import UIKit;
@import GLKit;
#include "RemoteViewController.h"


@interface RemoteView : GLKView
-(instancetype) initWithFrame:(CGRect)frame;
-(void) setRemoteBufferSize:(CGSize)size;
-(void) fillRemoteBuffer:(const uint8_t*)data frame:(CGRect)frame;
@property RemoteViewController* viewController;
@end
