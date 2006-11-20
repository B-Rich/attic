//
//  TextViewStream.mm
//  Attic
//
//  Created by John Wiegley on 11/19/06.
//  Copyright 2006 __MyCompanyName__. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "TextViewStream.h"

#include <cstring>

TextViewBuffer::TextViewBuffer(NSTextView * _textView) : textView(_textView)
{
  [textView retain];
  [textView setString:@""];
}

TextViewBuffer::~TextViewBuffer()
{
  [textView release];
}

void TextViewBuffer::InsertText(const char * text, unsigned int len)
{
  NSString * newOutput;

  char * buf = new char[len + 1];
  std::strncpy(buf, text, len);
  buf[len] = 0;

  // Insert the output into the outputView
  newOutput = [[NSString alloc] initWithCString:buf
	       encoding:NSMacOSRomanStringEncoding];

  delete[] buf;

  NSRange endRange;
  endRange.location = [[textView textStorage] length];
  endRange.length = 0;
  [textView replaceCharactersInRange:endRange withString:newOutput];
  endRange.length = [newOutput length];
  [textView scrollRangeToVisible:endRange];

  [newOutput release]; 
}

TextViewBuffer::int_type TextViewBuffer::overflow (int_type c)
{
  if (c != EOF) {
    char z = c;
    InsertText(&z, 1);
  }
  return c;
}

// write multiple characters
std::streamsize TextViewBuffer::xsputn (const char* s, std::streamsize num)
{
  InsertText(s, num);
  return num;
}
