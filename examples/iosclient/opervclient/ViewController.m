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
    UIButton* connectButton;
    UILabel* ipLabel;
    UITextField* ipTextField;
    UILabel* portLabel;
    UITextField* portTextField;
    UILabel* passwordLabel;
    UITextField* passwordTextField;
    UIToolbar* toolBar;
}
@end

@implementation ViewController

-(void) viewDidLoad
{
    [super viewDidLoad];
   
 	ipLabel = [[UILabel alloc] init];
    [ipLabel setText:@"Server address"];
    [ipLabel sizeToFit];
    CGFloat y = self.view.bounds.size.height/4;
    [ipLabel setFrame:CGRectMake((self.view.bounds.size.width - ipLabel.frame.size.width)/2, y, ipLabel.frame.size.width, ipLabel.frame.size.height)];
	[self.view addSubview:ipLabel];

    ipTextField = [[UITextField alloc] init];
    [ipTextField setPlaceholder:@"Server ip"];
    [ipTextField sizeToFit];
    [ipTextField setBorderStyle:UITextBorderStyleRoundedRect];
    [ipTextField setTextAlignment:NSTextAlignmentCenter];
    y += ipLabel.frame.size.height + 10;
    [ipTextField setFrame:CGRectMake((self.view.bounds.size.width / 2 - 60), y, 120, ipTextField.frame.size.height)];
	[self.view addSubview:ipTextField];
    
    portLabel = [[UILabel alloc] init];
    [portLabel setText:@"Server port"];
    [portLabel sizeToFit];
    y += ipTextField.frame.size.height + 15;
    [portLabel setFrame:CGRectMake((self.view.bounds.size.width - portLabel.frame.size.width)/2, y, portLabel.frame.size.width, portLabel.frame.size.height)];
     [portLabel sizeToFit];
    [self.view addSubview:portLabel];

    portTextField = [[UITextField alloc] init];
    [portTextField setPlaceholder:@"port"];
    [portTextField setText:@"5900"];
    [portTextField sizeToFit];
    [portTextField setBorderStyle:UITextBorderStyleRoundedRect];
    [portTextField setTextAlignment:NSTextAlignmentCenter];
    y += portLabel.frame.size.height + 10;
    [portTextField setFrame:CGRectMake((self.view.bounds.size.width / 2 - 60), y, 120, portTextField.frame.size.height)];
    [self.view addSubview:portTextField];

    passwordLabel = [[UILabel alloc] init];
    [passwordLabel setText:@"Password"];
    [passwordLabel sizeToFit];
    y += portTextField.frame.size.height + 15;
    [passwordLabel setFrame:CGRectMake(self.view.bounds.size.width/2 - passwordLabel.frame.size.width/2, y, passwordLabel.frame.size.width, passwordLabel.frame.size.height)];
    [self.view addSubview:passwordLabel];
    
    passwordTextField = [[UITextField alloc] init];
    [passwordTextField setPlaceholder:@"Password"];
    [passwordTextField setSecureTextEntry:true];
    [passwordTextField sizeToFit];
    [passwordTextField setBorderStyle:UITextBorderStyleRoundedRect];
    [passwordTextField setTextAlignment:NSTextAlignmentCenter];
    y += passwordLabel.frame.size.height + 10;
    [passwordTextField setFrame:CGRectMake((self.view.bounds.size.width / 2 - 60), y, 120, passwordTextField.frame.size.height)];
    [self.view addSubview:passwordTextField];
    
    y += passwordTextField.frame.size.height + 20;
    connectButton = [[UIButton alloc] initWithFrame:CGRectMake((self.view.bounds.size.width / 2 - 60), y, 120, 43.0)];
    [connectButton setTitle:@"Connect" forState:UIControlStateNormal];
    [connectButton setBackgroundColor:UIColor.darkTextColor];
    [connectButton setTintColor:UIColor.blackColor];
    [connectButton addTarget:self action:@selector(showRemoteView) forControlEvents:UIControlEventTouchUpInside];
    [self.view addSubview:connectButton];
}

-(void) showRemoteView
{
    if (mRemoteView && toolBar) {
        [mRemoteView setHidden:false];
        [toolBar setHidden:false];
    }else{
        UIEdgeInsets inset = [[[UIApplication sharedApplication] delegate] window].safeAreaInsets;
        mRemoteView = [[RemoteView alloc] initWithFrame:CGRectMake(0, inset.bottom+40, self.view.bounds.size.width, self.view.bounds.size.height - (inset.bottom+40))];
        [self.view addSubview:mRemoteView];
        
        toolBar = [[UIToolbar alloc] initWithFrame:CGRectMake(0, inset.bottom, self.view.bounds.size.width, 40)];
        UIBarButtonItem *back = [[UIBarButtonItem alloc] initWithTitle:@"Disconnect" style:UIBarButtonItemStyleDone  target:nil action:@selector(closeConnection)];
        NSArray *items = [NSArray arrayWithObjects: back, nil];
        toolBar.items = items;
        [self.view addSubview:toolBar];
    }
    [self connect];
}

-(void) closeConnection
{
    [mOpenRVContext disconnect];
    [toolBar setHidden:true];
    [mRemoteView setHidden:true];
}

-(void) connect
{
    if (mOpenRVContext) {
        return;
    }
    if (![ipTextField.text  isEqual: @""] && ![portTextField.text  isEqual: @""] && ![passwordTextField.text  isEqual: @""]) {
        mOpenRVContext = [[OpenRVContext alloc] init];
        mOpenRVContext.delegate = self;
        [mOpenRVContext setCredentials:nil password:passwordTextField.text];
        [mOpenRVContext setPort:(uint16_t)[portTextField.text integerValue]];
        [mOpenRVContext connectToHost:ipTextField.text];
    }
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
}

-(void) openRVContextDidConnect:(OpenRVContext *)context
{
    [mRemoteView setRemoteBufferSize:context.framebufferSize];
    [mRemoteView setFrame:CGRectMake(0, mRemoteView.frame.origin.y, context.framebufferSize.width, context.framebufferSize.height)];
}
-(void) openRVContextConnectFailed:(OpenRVContext *)context withError:(const orv_error_t *)error
{
    [mRemoteView setHidden:false];
    [toolBar setHidden:false];
    UIAlertController *alert = [UIAlertController alertControllerWithTitle:@"Error" message:[NSString stringWithUTF8String:error->mErrorMessage] preferredStyle:UIAlertControllerStyleAlert];
    [alert addAction:([UIAlertAction actionWithTitle:@"Close" style:UIAlertActionStyleCancel handler:nil])];
    [self presentViewController:alert animated:true completion:nil];
}
-(void) openRVContextDidDisconnected:(OpenRVContext *)context
{
    [toolBar setHidden:true];
    [mRemoteView setHidden:true];
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
