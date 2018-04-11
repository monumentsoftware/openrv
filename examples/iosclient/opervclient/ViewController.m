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

@interface ViewController ()
{
    InputFields* inputFields;
}
@end

@implementation ViewController

-(void) viewDidLoad
{
    [super viewDidLoad];
    inputFields = [[InputFields alloc] initWithFrame:CGRectMake(0, self.view.bounds.size.height/3, self.view.bounds.size.width, self.view.bounds.size.height*3/4) viewController:self];
    [self.view addSubview:inputFields];
}
-(void) connect:(NSString*)ip port:(NSString*)port password:(NSString*)password
{
    if (_mOpenRVContext) {
        return;
    }
    if (![ip  isEqual: @""] && ![port isEqual: @""] && ![password isEqual: @""]) {
        _mOpenRVContext = [[OpenRVContext alloc] init];
        
        RemoteViewController* remoteCtrl = [[RemoteViewController alloc] initViewController:self];
        [self presentViewController:remoteCtrl animated:true completion:nil];
        
        _mOpenRVContext.delegate = remoteCtrl;
        [_mOpenRVContext setCredentials:nil password:password];
        [_mOpenRVContext setPort:(uint16_t)[port integerValue]];
        [_mOpenRVContext connectToHost:ip];
    }
}
- (void)viewDidAppear:(BOOL)animated
{
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


@end
