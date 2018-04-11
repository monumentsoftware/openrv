//
//  ViewController.h
//  opervclient
//
//  Created by christoph on 03.04.18.
//  Copyright © 2018 Christoph Möde. All rights reserved.
//

#import <UIKit/UIKit.h>
@import OpenRV;

@interface ViewController : UIViewController
-(void) connect:(NSString*)ip port:(NSString*)port password:(NSString*)password;
@property OpenRVContext* mOpenRVContext;
@end

