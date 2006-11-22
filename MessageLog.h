#ifndef _MESSAGELOG_H
#define _MESSAGELOG_H

#include <iostream>
#include <sstream>
#include <deque>

#include <assert.h>
#include <boost/thread.hpp>

namespace Attic {

extern boost::mutex io_mutex;

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
  
  explicit MessageLog(std::ostream& _outs)
    : outs(_outs), terminateQueue(false) {}

  MessageLog(const MessageLog& other)
    : outs(other.outs), terminateQueue(other.terminateQueue) {}

private:
  typedef std::pair<Kind, std::string> messageQueuePair;
  typedef std::deque<messageQueuePair> messageQueueList;

  messageQueueList messageQueue;

  boost::mutex	   queueLock;
  boost::condition queueCond;

  bool terminateQueue;

public:
  void PostMessage(Kind messageKind, const std::string& messageText) {
    boost::mutex::scoped_lock lock(queueLock);
    messageQueue.push_back(messageQueuePair(messageKind, messageText));
    queueCond.notify_one();
  }

  void operator()() {
    boost::mutex::scoped_lock lock(queueLock);
    do {
      while (messageQueue.size() == 0)
	queueCond.wait(lock);

      for (messageQueueList::iterator i = messageQueue.begin();
	   i != messageQueue.end();
	   i++)
	InsertMessage((*i).first, (*i).second);
    }
    while (! terminateQueue);

    outs.flush();
  }

  void EndQueue() {
    terminateQueue = true;
    queueCond.notify_one();
  }

  void InsertMessage(Kind messageKind, const std::string& messageText) {
    boost::mutex::scoped_lock lock(io_mutex);

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
    (log).PostMessage(MessageLog::kind, buf.str());	\
  }

} // namespace Attic

#endif // _MESSAGELOG_H
