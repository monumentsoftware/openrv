//
//  RemoteViewController.h
//  OpenRVClient
//
//  Created by Niklas Wende on 11.04.18.
//  Copyright © 2018 Christoph Möde. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "ViewController.h"
@import OpenRV;

@interface RemoteViewController : UIViewController<OpenRVContextDelegate>
-(instancetype) initViewController:(ViewController*)viewController;
-(void) sendTouchEvents:(NSSet<UITouch *> *)touches click:(bool)clickdown;
@property OpenRVContext* openRVContext;
@end
