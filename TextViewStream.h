//
//  TextViewStream.h
//  Attic
//
//  Created by John Wiegley on 11/19/06.
//  Copyright 2006 __MyCompanyName__. All rights reserved.
//

#include <ostream>
#include <streambuf>

class NSTextView;
class TextViewBuffer : public std::streambuf
{
protected:
	NSTextView * textView;
public:
	// constructor
	TextViewBuffer (NSTextView * _textView);
	virtual ~TextViewBuffer();

protected:
	void InsertText(const char * text, unsigned int len);

	// write one character
	virtual int_type overflow (int_type c);

	// write multiple characters
	virtual std::streamsize xsputn (const char* s, std::streamsize num);
};

class TextViewStream : public std::ostream
{
protected:
	TextViewBuffer buf;
public:
	TextViewStream(NSTextView * textView) : buf(textView) {
		rdbuf(&buf);
	}
};
