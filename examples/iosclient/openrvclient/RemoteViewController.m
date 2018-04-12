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
    OpenRVContext* mOpenRVContext;
    BOOL mIsConntected;
    UITextField* mTextField;
}
@end

@implementation RemoteViewController
- (void)viewDidLoad {
    [super viewDidLoad];
    mIsConntected = false;
    mRemoteView = [[RemoteView alloc] init];
    mRemoteView.viewController = self;
    [self.view addSubview:mRemoteView];
    
    mToolBar = [[UIToolbar alloc] init];
    UIBarButtonItem *back = [[UIBarButtonItem alloc] initWithTitle:@"Disconnect" style:UIBarButtonItemStyleDone  target:nil action:@selector(closeConnection)];
    UIBarButtonItem *keyboard = [[UIBarButtonItem alloc] initWithTitle:@"Keyboard" style:UIBarButtonItemStyleDone  target:nil action:@selector(showKeyboard)];
    NSArray *items = [NSArray arrayWithObjects: back, keyboard, nil];
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
    UIEdgeInsets inset = [[[UIApplication sharedApplication] delegate] window].safeAreaInsets;
    CGFloat toolbarHeight = 40;
    if (mIsConntected) {
        CGFloat r = mOpenRVContext.framebufferSize.width/mOpenRVContext.framebufferSize.height;
        if (UIDevice.currentDevice.orientation == UIDeviceOrientationLandscapeRight || UIDevice.currentDevice.orientation == UIDeviceOrientationLandscapeLeft) {
            [mRemoteView setFrame:CGRectMake((self.view.bounds.size.width - (self.view.bounds.size.height-toolbarHeight)*r)/2, toolbarHeight, self.view.bounds.size.height*r, self.view.bounds.size.height-toolbarHeight)];
        } else {
            r = mOpenRVContext.framebufferSize.height/mOpenRVContext.framebufferSize.width;
            [mRemoteView setFrame:CGRectMake(0, (self.view.bounds.size.height - self.view.bounds.size.width*r)/2, self.view.bounds.size.width, self.view.bounds.size.width*r)];
        }
    }
    [mToolBar setFrame:CGRectMake(0, UIDevice.currentDevice.orientation == UIDeviceOrientationPortrait ? inset.bottom : 0, self.view.bounds.size.width, toolbarHeight)];
}
-(void) sendTouchEvents:(NSSet<UITouch *> *)touches click:(bool)clickdown
{
    for (UITouch *t in touches) {
        CGPoint p = [t locationInView:mRemoteView];
        CGSize screenSize = mOpenRVContext.framebufferSize;
        CGSize remoteViewSize = mRemoteView.bounds.size;
        int32_t x = (p.x / remoteViewSize.width) * screenSize.width;
        int32_t y = (p.y / remoteViewSize.height) * screenSize.height;
        [mOpenRVContext sendPointerEvent:x y:y buttonMask: clickdown == true ? 1 : 0];
    }
}
- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}
-(void) show:(OpenRVContext*)connectionContext
{
    mOpenRVContext = connectionContext;
}
-(void) closeConnection
{
    [self dismissViewControllerAnimated:true completion:nil];
    [mOpenRVContext disconnect];
}
-(void) showKeyboard
{
    if (!mTextField) {
        mTextField = [UITextField new];
        mTextField.frame = CGRectMake(-100, -100, 50, 50);
        mTextField.autocorrectionType = UITextAutocorrectionTypeNo;
        mTextField.autocapitalizationType = UITextAutocapitalizationTypeNone;
        mTextField.spellCheckingType = UITextSpellCheckingTypeNo;
        mTextField.keyboardType = UIKeyboardTypeDefault;
        mTextField.delegate = self;
        mTextField.text = @"dummy";
        [self.view addSubview:mTextField];
        [mTextField becomeFirstResponder];
    } else {
        [mTextField removeFromSuperview];
        mTextField = nil;
    }
}
- (BOOL)textField:(UITextField *)textField shouldChangeCharactersInRange:(NSRange)range replacementString:(NSString *)string
{
    if (string.length == 1) {
        unichar c = [string characterAtIndex:0];
        [self sendKeyEvent:c];
    }
    else if (string.length == 0) {
        [self sendKeyEvent:8];
    }
    return NO;
}
-(void) sendKeyEvent:(unichar)key
{
    if (key == 8) { // backspace
        [mOpenRVContext sendKeyEvent:0xff08 isDown:1];
        [mOpenRVContext sendKeyEvent:0xff08 isDown:0];
    } else {
        [mOpenRVContext sendKeyEvent:key isDown:1];
        [mOpenRVContext sendKeyEvent:key isDown:0];
    }
}
- (BOOL)textFieldShouldReturn:(UITextField *)textField
{
    return YES;
}
-(void) openRVContextDidConnect:(OpenRVContext *)context
{
    mIsConntected = true;
    [mRemoteView setRemoteBufferSize:context.framebufferSize];
    CGFloat r = context.framebufferSize.height/context.framebufferSize.width;
    [mRemoteView setFrame:CGRectMake(0, (self.view.bounds.size.height - self.view.bounds.size.width*r)/2, self.view.bounds.size.width, self.view.bounds.size.width*r)];
    CGSize screenSize = mOpenRVContext.framebufferSize;
    int32_t x = screenSize.width/2;
    int32_t y = screenSize.height/2;
    [mOpenRVContext sendPointerEvent:x y:y buttonMask: 0];
    [mOpenRVContext sendPointerEvent:x y:y buttonMask: 0];
}
-(void) openRVContextConnectFailed:(OpenRVContext *)context withError:(const orv_error_t *)error
{
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"Error" message:[NSString stringWithUTF8String:error->mErrorMessage] preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:([UIAlertAction actionWithTitle:@"Close" style:UIAlertActionStyleCancel handler:^(UIAlertAction * _Nonnull action) {
        [self->_viewController dismissViewControllerAnimated:true completion:nil];
    }])];
    [self presentViewController:alert animated:true completion:nil];
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
