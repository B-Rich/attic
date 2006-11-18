#ifndef _MANAGER_H
#define _MANAGER_H

#include "DataPool.h"

#include <boost/thread.hpp>

namespace Attic {

class DataPool;
class RunPoolJob
{
public:
  DataPool *  Pool;
  MessageLog& Log;

  RunPoolJob(DataPool * _Pool, MessageLog& _Log)
    : Pool(_Pool), Log(_Log) {}

  void operator()() {
    assert(Pool);
    Pool->ComputeChanges();
    Pool->ApplyChanges(Log);
  }
};

class Manager
{
public:
  boost::thread_group    ActiveJobs;
  std::deque<DataPool *> CurrentPools;
  MessageLog&            CurrentLog;

  Manager(MessageLog& log) : CurrentLog(log) {}

  ~Manager() {
    for (std::deque<DataPool *>::iterator i = CurrentPools.begin();
	 i != CurrentPools.end();
	 i++)
      delete *i;
  }

  DataPool * CreatePool() {
    DataPool * newPool = new DataPool;
    CurrentPools.push_back(newPool);
    return newPool;
  }

  void operator()() {
    for (std::deque<DataPool *>::iterator i = CurrentPools.begin();
	 i != CurrentPools.end();
	 i++)
      ActiveJobs.create_thread(RunPoolJob(*i, CurrentLog));
    ActiveJobs.join_all();
  }
  void Synchronize() { (*this)(); }
};

} // namespace Attic

#endif // _MANAGER_H
