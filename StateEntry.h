#ifndef _STATEENTRY_H
#define _STATEENTRY_H

#include <map>

#include "FileInfo.h"

namespace Attic {

class StateMap;
class StateEntry
{
public:
  FileInfo     Info;
  int	       Depth;
  static bool  TrustMode;
  StateMap *   StateMapObj;
  StateEntry * Parent;

  typedef std::map<std::string, StateEntry *>  ChildrenMap;
  typedef std::pair<std::string, StateEntry *> ChildrenPair;

  ChildrenMap Children;

  StateEntry(StateMap *     _StateMapObj,
	     StateEntry *    _Parent,
	     const FileInfo& _Info = FileInfo())
    : Handled(false), DeletePending(false),
      Depth(_Parent ? _Parent->Depth + 1 : 0),
      StateMapObj(_StateMapObj), Parent(_Parent), Info(_Info)
  {
    if (Parent != NULL)
      Parent->AppendChild(this);
  }

  StateEntry(StateMap *     _StateMapObj,
	     StateEntry *    _Parent,
	     const std::string& path)
    : Handled(false), DeletePending(false),
      Depth(_Parent ? _Parent->Depth + 1 : 0),
      StateMapObj(_StateMapObj), Parent(_Parent), Info(path)
  {
    if (Parent != NULL)
      Parent->AppendChild(this);
  }

  void WriteTo(std::ostream& w, bool top);

  static StateEntry *
  LoadElement(StateMap * StateMapObj, StateEntry * Parent,
	      const std::string& parentPath,
	      const std::string& relativePath,
	      char *& data);

  void Move(const std::string& target);
  void Copy(const std::string& target);
  bool Copy(StateEntry * dirToCopyInto, bool moveOnly);
  void CopyDirectory(const std::string& target, bool andChildren);
  void CompareTo(StateEntry * other, StateEntry * target = NULL);
  void MarkDeletePending();
  void Report() const;

  void Delete() {
    Info.Delete();
  }
  bool Exists() const {
    return Info.Exists();
  }
};

} // namespace Attic

#endif // _STATEENTRY_H
