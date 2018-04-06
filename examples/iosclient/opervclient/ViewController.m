//
//  ViewController.m
//  opervclient
//
//  Created by christoph on 03.04.18.
//  Copyright © 2018 Christoph Möde. All rights reserved.
//

#import "ViewController.h"
#import "RemoteView.h"

@interface ViewController ()
{
    RemoteView* mRemoteView;
    OpenRVContext* mOpenRVContext;
}
@end

@implementation ViewController

-(void) viewDidLoad
{
    [super viewDidLoad];
    mRemoteView = [[RemoteView alloc] initWithFrame:self.view.bounds];
    [self.view addSubview:mRemoteView];

    [self connect];
}

-(void) connect
{
    if (mOpenRVContext) {
        return;
    }
    mOpenRVContext = [[OpenRVContext alloc] init];
    mOpenRVContext.delegate = self;
    [mOpenRVContext connectToHost:@"176.9.90.194"];
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
}

-(void) contextDidConnect:(OpenRVContext*)context withError:(const orv_error_t*)error
{

}
-(void) contextDidDisconnected:(OpenRVContext*)context
{

}
-(void) contextFramebufferUpdated:(OpenRVContext*)context frame:(CGRect)frame
{

}

@end
