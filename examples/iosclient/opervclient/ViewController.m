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
    [mOpenRVContext setCredentials:nil password:@""];
    [mOpenRVContext connectToHost:@""];
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
}

-(void) openRVContextDidConnect:(OpenRVContext *)context
{
    [mRemoteView setRemoteBufferSize:context.framebufferSize];
}
-(void) openRVContextConnectFailed:(OpenRVContext *)context withError:(const orv_error_t *)error
{
}
-(void) openRVContextDidDisconnected:(OpenRVContext *)context
{
}
-(void) openRVContextFramebufferUpdated:(OpenRVContext *)context frame:(CGRect)frame
{
    OpenRVFramebuffer* fb = [mOpenRVContext lockFramebuffer];
    if (!fb) {
        NSLog(@"ERROR failed to lock framebuffer");
        return;
    }
    [mRemoteView fillRemoteBuffer:fb.buffer frame:frame];
    [mOpenRVContext unlockFramebuffer];
    [mRemoteView setNeedsDisplay];
}
@end
