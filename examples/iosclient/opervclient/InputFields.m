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
UIButton* connectButton;
UILabel* ipLabel;
UITextField* ipTextField;
UILabel* portLabel;
UITextField* portTextField;
UILabel* passwordLabel;
UITextField* passwordTextField;
}
-(instancetype) initWithFrame:(CGRect)frame viewController:(ViewController*)viewController
{
    self = [super initWithFrame:frame];
    if (self) {
        _mViewController = viewController;
        ipLabel = [[UILabel alloc] init];
        [ipLabel setText:@"Server address"];
        [ipLabel sizeToFit];
        CGFloat y = 0;
        [ipLabel setFrame:CGRectMake((self.bounds.size.width - ipLabel.frame.size.width)/2, y, ipLabel.frame.size.width, ipLabel.frame.size.height)];
        [self addSubview:ipLabel];
        
        ipTextField = [[UITextField alloc] init];
        [ipTextField setPlaceholder:@"Server ip"];
        [ipTextField sizeToFit];
        [ipTextField setBorderStyle:UITextBorderStyleRoundedRect];
        [ipTextField setTextAlignment:NSTextAlignmentCenter];
        y += ipLabel.frame.size.height + 10;
        [ipTextField setFrame:CGRectMake((self.bounds.size.width / 2 - 60), y, 120, ipTextField.frame.size.height)];
        [self addSubview:ipTextField];
        
        portLabel = [[UILabel alloc] init];
        [portLabel setText:@"Server port"];
        [portLabel sizeToFit];
        y += ipTextField.frame.size.height + 15;
        [portLabel setFrame:CGRectMake((self.bounds.size.width - portLabel.frame.size.width)/2, y, portLabel.frame.size.width, portLabel.frame.size.height)];
        [portLabel sizeToFit];
        [self addSubview:portLabel];
        
        portTextField = [[UITextField alloc] init];
        [portTextField setPlaceholder:@"port"];
        [portTextField setText:@"5900"];
        [portTextField sizeToFit];
        [portTextField setBorderStyle:UITextBorderStyleRoundedRect];
        [portTextField setTextAlignment:NSTextAlignmentCenter];
        y += portLabel.frame.size.height + 10;
        [portTextField setFrame:CGRectMake((self.bounds.size.width / 2 - 60), y, 120, portTextField.frame.size.height)];
        [self addSubview:portTextField];
        
        passwordLabel = [[UILabel alloc] init];
        [passwordLabel setText:@"Password"];
        [passwordLabel sizeToFit];
        y += portTextField.frame.size.height + 15;
        [passwordLabel setFrame:CGRectMake(self.bounds.size.width/2 - passwordLabel.frame.size.width/2, y, passwordLabel.frame.size.width, passwordLabel.frame.size.height)];
        [self addSubview:passwordLabel];
        
        passwordTextField = [[UITextField alloc] init];
        [passwordTextField setPlaceholder:@"Password"];
        [passwordTextField setSecureTextEntry:true];
        [passwordTextField sizeToFit];
        [passwordTextField setBorderStyle:UITextBorderStyleRoundedRect];
        [passwordTextField setTextAlignment:NSTextAlignmentCenter];
        y += passwordLabel.frame.size.height + 10;
        [passwordTextField setFrame:CGRectMake((self.bounds.size.width / 2 - 60), y, 120, passwordTextField.frame.size.height)];
        [self addSubview:passwordTextField];
        
        y += passwordTextField.frame.size.height + 20;
        connectButton = [[UIButton alloc] initWithFrame:CGRectMake((self.bounds.size.width / 2 - 60), y, 120, 43.0)];
        [connectButton setTitle:@"Connect" forState:UIControlStateNormal];
        [connectButton setBackgroundColor:UIColor.darkTextColor];
        [connectButton setTintColor:UIColor.blackColor];
        [connectButton addTarget:self action:@selector(showRemoteView) forControlEvents:UIControlEventTouchUpInside];
        [self addSubview:connectButton];
    }
    return self;
}

-(void) showRemoteView {
    [_mViewController connect:ipTextField.text port:portTextField.text password:passwordTextField.text];
}

@end
