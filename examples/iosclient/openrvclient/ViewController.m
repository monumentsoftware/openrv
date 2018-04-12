//
//  ViewController.m
//  opervclient
//
//  Created by christoph on 03.04.18.
//  Copyright © 2018 Christoph Möde. All rights reserved.
//

#import "ViewController.h"
#import "RemoteView.h"
#import "InputFields.h"
#import "AppDelegate.h"

@interface ViewController ()
{
    InputFields* inputFields;
}
@end

@implementation ViewController

-(void) viewDidLoad
{
    [super viewDidLoad];
    UIEdgeInsets inset = [[[UIApplication sharedApplication] delegate] window].safeAreaInsets;
    inputFields = [[InputFields alloc] initWithFrame:CGRectMake(0, 10 + inset.bottom, self.view.bounds.size.width, self.view.bounds.size.height*2/3 + inset.bottom) viewController:self];
    [self.view addSubview:inputFields];
}
-(void) connect:(NSString*)ip port:(NSString*)port password:(NSString*)password
{
    if (_openRVContext) {
        return;
    }
    if (![ip  isEqual: @""] && ![port isEqual: @""] && ![password isEqual: @""]) {
        _openRVContext = [[OpenRVContext alloc] init];
        
        RemoteViewController* remoteCtrl = [[RemoteViewController alloc] initViewController:self];
        [self presentViewController:remoteCtrl animated:true completion:nil];
        
        _openRVContext.delegate = remoteCtrl;
        [_openRVContext setCredentials:nil password:password];
        [_openRVContext setPort:(uint16_t)[port integerValue]];
        [_openRVContext connectToHost:ip];
    }
}
- (void)viewDidAppear:(BOOL)animated
{
    [self shouldRotate:false];
}
- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
}
-(BOOL)shouldAutorotate
{
    return NO;
}
-(UIInterfaceOrientationMask)supportedInterfaceOrientations
{
    return UIInterfaceOrientationMaskPortrait;
}
-(void) shouldRotate:(bool) rotate
{
    AppDelegate* appDelegate = (AppDelegate*)[UIApplication sharedApplication].delegate;
    appDelegate.shouldRotate = rotate;
}


@end
