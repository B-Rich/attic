#ifndef _STATEMAP_H
#define _STATEMAP_H

#include "Regex.h"
#include "FileInfo.h"
#include "StateChange.h"

#include <map>
#include <deque>

namespace Attic {

class Location;
class StateMap
{
public:
  FileInfo * Root;

  typedef std::map<md5sum_t, std::deque<FileInfo *> >  ChecksumMap;
  typedef std::pair<md5sum_t, std::deque<FileInfo *> > ChecksumPair;

  ChecksumMap EntriesByChecksum;

  StateMap(FileInfo * _Root = NULL) : Root(_Root) {}
  ~StateMap() {
    if (Root) delete Root;
  }

  void RegisterChecksums(FileInfo * entry);
  void LoadFrom(const Path& path);
  void SaveTo(std::ostream& out);

  void AddTopLevel(FileInfo * info) {
    if (! Root)
      Root = new FileInfo;
    Root->AddChild(info);
  }

  std::deque<FileInfo *> * FindDuplicate(FileInfo * item);

  void CompareTo(const StateMap * ancestor, StateChangesMap& changesMap) const {
    CompareFiles(Root, ancestor->Root, changesMap);
  }
};

} // namespace Attic

#endif // _STATEMAP_H
