//
//  MainWindowController.h
//  Attic
//
//  Created by John Wiegley on 11/19/06.
//  Copyright 2006 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>


@interface MainWindowController : NSWindowController {
  IBOutlet NSTextView * debugView;
}

- (IBAction)Synchronize:(id)sender;

@end
