#ifndef _STATECHANGE_H
#define _STATECHANGE_H

#include "MessageLog.h"
#include "FileInfo.h"

namespace Attic {

class FileInfo;
class StateChange
{
public:
  enum Kind {
    Nothing, Add, Remove, Update, UpdateProps
  };

  StateChange *	Next;
  Kind		ChangeKind;
  FileInfo *	Item;

  union {
    FileInfo *	    Ancestor;
    FileInfoArray * Duplicates;
  };

  StateChange(Kind _ChangeKind, FileInfo * _Item, FileInfo * _Ancestor)
    : Next(NULL), ChangeKind(_ChangeKind), Item(_Item), Ancestor(_Ancestor) {}

  void Report(MessageLog& log) const;
  void DebugPrint(MessageLog& log) const;
  int  Depth() const;
};

} // namespace Attic

#endif // _STATECHANGE_H
