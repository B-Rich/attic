//
//  MainWindowController.mm
//  Attic
//
//  Created by John Wiegley on 11/19/06.
//  Copyright 2006 __MyCompanyName__. All rights reserved.
//

#import "MainWindowController.h"
#import "TextViewStream.h"
#import "Invoke.h"

@implementation MainWindowController

- (IBAction)Synchronize:(id)sender
{
	TextViewStream viewStream(debugView);
	Attic::MessageLog log(viewStream);
	InvokeSychronization(log);
}

@end
