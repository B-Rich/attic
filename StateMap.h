#ifndef _STATEMAP_H
#define _STATEMAP_H

#include "Regex.h"
#include "FileInfo.h"
#include "ChangeSet.h"

#include <map>
#include <deque>

namespace Attic {

class Location;
class StateMap
{
public:
  FileInfo * Root;

  typedef std::map<md5sum_t, FileInfoArray>  ChecksumMap;
  typedef std::pair<md5sum_t, FileInfoArray> ChecksumPair;

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

  FileInfoArray * FindDuplicate(const FileInfo * item);

  void ApplyChanges(const ChangeSet& changeSet) {
    for (ChangeSet::ChangesMap::const_iterator i = changeSet.Changes.begin();
	 i != changeSet.Changes.end();
	 i++)
      ApplyChange(*(*i).second);
  }

  void ApplyChange(const StateChange& change);
};

} // namespace Attic

#endif // _STATEMAP_H
