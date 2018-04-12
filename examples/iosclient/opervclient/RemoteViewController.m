//
//  RemoteViewController.m
//  OpenRVClient
//
//  Created by Niklas Wende on 11.04.18.
//  Copyright © 2018 Christoph Möde. All rights reserved.
//

#import "RemoteViewController.h"
#import "RemoteView.h"
#import "AppDelegate.h"
@interface RemoteViewController (){
    RemoteView* mRemoteView;
    UIToolbar* mToolBar;
    ViewController* mViewController;
}
@end

@implementation RemoteViewController

-(instancetype) initViewController:(ViewController*)viewController
{
    self = [super init];
    if (self) {
        mViewController = viewController;
    }else{
        return nil;
    }
    return self;
}
- (void)viewDidLoad {
    [super viewDidLoad];
    mRemoteView = [[RemoteView alloc] init];
    mRemoteView.viewController = self;
    [self.view addSubview:mRemoteView];
    
    mToolBar = [[UIToolbar alloc] init];
    UIBarButtonItem *back = [[UIBarButtonItem alloc] initWithTitle:@"Disconnect" style:UIBarButtonItemStyleDone  target:nil action:@selector(closeConnection)];
    NSArray *items = [NSArray arrayWithObjects: back, nil];
    mToolBar.items = items;
    [self.view addSubview:mToolBar];
    [NSNotificationCenter.defaultCenter addObserver:self selector:@selector(rotated) name:UIDeviceOrientationDidChangeNotification object:nil];
    [self rotated];
}
- (void)viewDidAppear:(BOOL)animated
{
    [self shouldRotate:true];
}
-(void) rotated
{
    if (UIDevice.currentDevice.orientation == UIDeviceOrientationLandscapeRight || UIDevice.currentDevice.orientation == UIDeviceOrientationLandscapeLeft) {
        [mRemoteView setFrame:CGRectMake(0, 40, self.view.bounds.size.width, self.view.bounds.size.height - 40)];
        [mToolBar setFrame:CGRectMake(0, 0, self.view.bounds.size.width, 40)];
    } else {
        UIEdgeInsets inset = [[[UIApplication sharedApplication] delegate] window].safeAreaInsets;
        [mRemoteView setFrame:CGRectMake(0, inset.bottom+40, self.view.bounds.size.width, self.view.bounds.size.height - (inset.bottom+40))];
        [mToolBar setFrame:CGRectMake(0, inset.bottom, self.view.bounds.size.width, 40)];
    }
   
}
-(void) sendTouchEvents:(NSSet<UITouch *> *)touches click:(bool)clickdown
{
    for (UITouch *t in touches) {
        CGPoint p = [t locationInView:mRemoteView];
        CGSize screenSize = mViewController.openRVContext.framebufferSize;
        CGSize remoteViewSize = mRemoteView.bounds.size;
        int32_t x = (p.x / remoteViewSize.width) * screenSize.width;
        int32_t y = (p.y / remoteViewSize.height) * screenSize.height;
        [mViewController.openRVContext sendPointerEvent:x y:y buttonMask: clickdown == true ? 1 : 0];
    }
}
- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}
-(void) closeConnection
{
    [self dismissViewControllerAnimated:true completion:nil];
    [mViewController.openRVContext disconnect];
}
-(void) openRVContextDidConnect:(OpenRVContext *)context
{
    [mRemoteView setRemoteBufferSize:context.framebufferSize];
    CGFloat r = context.framebufferSize.height/context.framebufferSize.width;
    [mRemoteView setFrame:CGRectMake(0, (self.view.bounds.size.height - self.view.bounds.size.width*r)/2, self.view.bounds.size.width, self.view.bounds.size.width*r)];
}
-(void) openRVContextConnectFailed:(OpenRVContext *)context withError:(const orv_error_t *)error
{
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"Error" message:[NSString stringWithUTF8String:error->mErrorMessage] preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:([UIAlertAction actionWithTitle:@"Close" style:UIAlertActionStyleCancel handler:nil])];
    [self presentViewController:alert animated:true completion:nil];
    [self dismissViewControllerAnimated:true completion:^{
        [self dismissViewControllerAnimated:true completion:nil];
    }];
}
-(void) openRVContextDidDisconnected:(OpenRVContext *)context
{
   [self dismissViewControllerAnimated:true completion:nil];
}
-(void) openRVContextFramebufferUpdated:(OpenRVContext *)context frame:(CGRect)frame
{
    OpenRVFramebuffer* fb = [context lockFramebuffer];
    if (!fb) {
        NSLog(@"ERROR failed to lock framebuffer");
        return;
    }
    [mRemoteView fillRemoteBuffer:fb.buffer frame:frame];
    [context unlockFramebuffer];
    [mRemoteView setNeedsDisplay];
}
-(void) shouldRotate:(bool) rotate
{
    AppDelegate* appDelegate = (AppDelegate*)[UIApplication sharedApplication].delegate;
    appDelegate.shouldRotate = rotate;
}

/*
#pragma mark - Navigation

// In a storyboard-based application, you will often want to do a little preparation before navigation
- (void)prepareForSegue:(UIStoryboardSegue *)segue sender:(id)sender {
    // Get the new view controller using [segue destinationViewController].
    // Pass the selected object to the new view controller.
}
*/

@end
