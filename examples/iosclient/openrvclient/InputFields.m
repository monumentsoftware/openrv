//
//  InputFields.m
//  OpenRVClient
//
//  Created by Niklas Wende on 11.04.18.
//  Copyright © 2018 Christoph Möde. All rights reserved.
//

#import "InputFields.h"
@implementation InputFields
{
    UIButton* mConnectButton;
    UILabel* mIpLabel;
    UITextField* mIpTextField;
    UILabel* mPortLabel;
    UITextField* mPortTextField;
    UILabel* mPasswordLabel;
    UITextField* mPasswordTextField;
}
-(instancetype) initWithFrame:(CGRect)frame viewController:(ViewController*)viewController
{
    self = [super initWithFrame:frame];
    if (self) {
        _viewController = viewController;
        mIpLabel = [[UILabel alloc] init];
        [mIpLabel setText:@"Server address"];
        [mIpLabel sizeToFit];
        CGFloat y = 0;
        [mIpLabel setFrame:CGRectMake((self.bounds.size.width - mIpLabel.frame.size.width)/2, y, mIpLabel.frame.size.width, mIpLabel.frame.size.height)];
        [self addSubview:mIpLabel];
        
        mIpTextField = [[UITextField alloc] init];
        [mIpTextField setDelegate:(id)self];
        [mIpTextField setPlaceholder:@"Server ip"];
        [mIpTextField sizeToFit];
        [mIpTextField setBorderStyle:UITextBorderStyleRoundedRect];
        [mIpTextField setTextAlignment:NSTextAlignmentCenter];
        y += mIpLabel.frame.size.height + 20;
        [mIpTextField setFrame:CGRectMake(15, y, (self.bounds.size.width - 30), mIpTextField.frame.size.height + 10)];
        [self addSubview:mIpTextField];
        
        mPortLabel = [[UILabel alloc] init];
        [mPortLabel setText:@"Server port"];
        [mPortLabel sizeToFit];
        y += mIpTextField.frame.size.height + 15;
        [mPortLabel setFrame:CGRectMake((self.bounds.size.width - mPortLabel.frame.size.width)/2, y, mPortLabel.frame.size.width, mPortLabel.frame.size.height)];
        [mPortLabel sizeToFit];
        [self addSubview:mPortLabel];
        
        mPortTextField = [[UITextField alloc] init];
        [mPortTextField setDelegate:(id)self];
        [mPortTextField setPlaceholder:@"port"];
        [mPortTextField setText:@"5900"];
        [mPortTextField sizeToFit];
        [mPortTextField setBorderStyle:UITextBorderStyleRoundedRect];
        [mPortTextField setTextAlignment:NSTextAlignmentCenter];
        y += mPortLabel.frame.size.height + 20;
        [mPortTextField setFrame:CGRectMake(15, y, (self.bounds.size.width - 30), mPortTextField.frame.size.height + 10)];
        [self addSubview:mPortTextField];
        
        mPasswordLabel = [[UILabel alloc] init];
        [mPasswordLabel setText:@"Password"];
        [mPasswordLabel sizeToFit];
        y += mPortTextField.frame.size.height + 15;
        [mPasswordLabel setFrame:CGRectMake(self.bounds.size.width/2 - mPasswordLabel.frame.size.width/2, y, mPasswordLabel.frame.size.width, mPasswordLabel.frame.size.height)];
        [self addSubview:mPasswordLabel];
        
        mPasswordTextField = [[UITextField alloc] init];
        [mPasswordTextField setDelegate:(id)self];
        [mPasswordTextField setPlaceholder:@"Password"];
        [mPasswordTextField setSecureTextEntry:true];
        [mPasswordTextField sizeToFit];
        [mPasswordTextField setBorderStyle:UITextBorderStyleRoundedRect];
        [mPasswordTextField setTextAlignment:NSTextAlignmentCenter];
        y += mPasswordLabel.frame.size.height + 20;
        [mPasswordTextField setFrame:CGRectMake(15, y, (self.bounds.size.width - 30), mPasswordTextField.frame.size.height + 10)];
        [self addSubview:mPasswordTextField];
        
        y += mPasswordTextField.frame.size.height + 20;
        mConnectButton = [[UIButton alloc] initWithFrame:CGRectMake((self.bounds.size.width / 2 - 60), y, 120, 43.0)];
        [mConnectButton setTitle:@"Connect" forState:UIControlStateNormal];
        [mConnectButton setBackgroundColor:UIColor.darkTextColor];
        [mConnectButton setTintColor:UIColor.blackColor];
        [mConnectButton addTarget:self action:@selector(showRemoteView) forControlEvents:UIControlEventTouchUpInside];
        [self addSubview:mConnectButton];
    }
    return self;
}

-(void) showRemoteView
{
    [_viewController connect:mIpTextField.text port:mPortTextField.text password:mPasswordTextField.text];
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField
{
    [self showRemoteView];
    [textField resignFirstResponder];
    return YES;
}

- (void)selectionWillChange:(nullable id <UITextInput>)textInput
{
}
- (void)selectionDidChange:(nullable id <UITextInput>)textInput
{
}
- (void)textWillChange:(nullable id <UITextInput>)textInput
{
}
- (void)textDidChange:(nullable id <UITextInput>)textInput
{
}
@end
