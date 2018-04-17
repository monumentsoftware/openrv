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

@interface RemoteViewController : UIViewController<OpenRVContextDelegate, UITextFieldDelegate, UIScrollViewDelegate>
-(void) sendTouchEvents:(NSSet<UITouch *> *)touches click:(bool)clickdown;
-(void) show:(OpenRVContext*)connectionContext;
@property ViewController* viewController;
@end
