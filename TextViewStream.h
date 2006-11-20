//
//  TextViewStream.h
//  Attic
//
//  Created by John Wiegley on 11/19/06.
//  Copyright 2006 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>

#include <ostream>

class TextViewStream : public std::ostream
{
	NSTextView * textView;
public:
	TextViewStream(NSTextView * _textView) : textView(_textView) {}

	TextViewStream& operator<<(const std::string& str) {
		[textView insertText:[NSString stringWithCString: str.c_str()]];
		return *this;
	}
};
