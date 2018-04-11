//
//  InputFields.h
//  OpenRVClient
//
//  Created by Niklas Wende on 11.04.18.
//  Copyright © 2018 Christoph Möde. All rights reserved.
//

#import <UIKit/UIKit.h>
#import "ViewController.h"

@interface InputFields : UIView
-(instancetype) initWithFrame:(CGRect)frame viewController:(ViewController*)viewController;
@property ViewController* mViewController;
@end
