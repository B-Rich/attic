#ifndef _STATEENTRY_H
#define _STATEENTRY_H

#include <map>

#include "FileInfo.h"

namespace Attic {

class FileState;
class StateEntry
{
public:
  FileInfo     Info;
  bool	       Handled;
  bool	       DeletePending;
  int	       Depth;
  static bool  TrustMode;
  FileState *  FileStateObj;
  StateEntry * Parent;

  typedef std::map<std::string, StateEntry *>  children_map;
  typedef std::pair<std::string, StateEntry *> children_pair;

  children_map Children;

  StateEntry(FileState *     _FileStateObj,
	     StateEntry *    _Parent,
	     const FileInfo& _Info = FileInfo())
    : Handled(false), DeletePending(false),
      Depth(_Parent ? _Parent->Depth + 1 : 0),
      FileStateObj(_FileStateObj), Parent(_Parent), Info(_Info)
  {
    if (Parent != NULL)
      Parent->AppendChild(this);
  }

  void AppendChild(StateEntry * entry)
  {
    std::pair<children_map::iterator, bool> result =
      Children.insert(children_pair(entry->Info.Name, entry));
    assert(result.second);
  }

  void WriteTo(std::ostream& w, bool top);

  static StateEntry *
  LoadElement(FileState * FileStateObj, StateEntry * Parent,
	      const std::string& parentPath,
	      const std::string& relativePath,
	      char *& data);

  StateEntry * CreateChild(const std::string& name);
  StateEntry * FindChild(const std::string& name);
  StateEntry * FindOrCreateChild(const std::string& name)
  {
    StateEntry * child = FindChild(name);
    if (child == NULL)
      child = CreateChild(name);
    return child;
  }

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
