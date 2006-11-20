#ifndef _MESSAGELOG_H
#define _MESSAGELOG_H

#include <iostream>
#include <sstream>

#include <assert.h>

namespace Attic {

class MessageLog
{
  std::ostream& outs;

public:
  enum Kind {
    Message, Note, Warn, Error, Fatal, Assert
  };
  
  enum MessageKind {
    None
  };
  
  MessageLog(std::ostream& _outs) : outs(_outs) {}
  ~MessageLog() {
    outs.flush();
  }

  void SendMessage(Kind messageKind, const std::string& messageText) {
    switch (messageKind) {
    case Message: break;
    case Note:    outs << "Note: "; break;
    case Warn:    outs << "Warning: "; break;
    case Error:   outs << "Error: "; break;
    case Fatal:   outs << "Fatal: "; break;
    case Assert:  outs << "Failed Assertion: "; break;
    default:      assert(0); break;
    }
    outs << messageText << std::endl;
  }
};

#define LOG(log, kind, stream) {			\
    std::ostringstream buf;				\
    buf << stream;					\
    (log).SendMessage(MessageLog::kind, buf.str());	\
  }

} // namespace Attic

#endif // _MESSAGELOG_H
