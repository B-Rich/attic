#ifndef _STATEMAP_H
#define _STATEMAP_H

#include "Regex.h"
#include "FileInfo.h"
#include "StateChange.h"

#include <map>

namespace Attic {

class Location;
class StateMap
{
public:
  FileInfo * Root;

  typedef std::map<md5sum_t, FileInfo *>  ChecksumMap;
  typedef std::pair<md5sum_t, FileInfo *> ChecksumPair;

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

  FileInfo * FindDuplicate(FileInfo * item);

  void CompareTo(const StateMap * ancestor, StateChangesMap& changesMap) const;
};

} // namespace Attic

#endif // _STATEMAP_H
