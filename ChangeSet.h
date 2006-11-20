#ifndef _CHANGESET_H
#define _CHANGESET_H

#include "MessageLog.h"
#include "StateChange.h"

#include <string>
#include <map>
#include <deque>

namespace Attic {

class Location;
class ChangeSet
{
  void PostAddChange(FileInfo * entry);
  void PostRemoveChange(FileInfo * entryParent, const std::string& childName,
			FileInfo * ancestorChild);

  inline void PostUpdateChange(FileInfo * entry, FileInfo * ancestor) {
    PostChange(StateChange::Update, entry, ancestor);
  }

  inline void PostUpdateAttrsChange(FileInfo * entry, FileInfo * ancestor) {
    PostChange(StateChange::UpdateAttrs, entry, ancestor);
  }

public:
  typedef std::map<std::string, StateChange *>  ChangesMap;
  typedef std::pair<std::string, StateChange *> ChangesPair;

  struct ChangeComparer {
    bool operator()(const StateChange * left,
		    const StateChange * right) const;
  };

  typedef std::deque<StateChange *> ChangesArray;

  ChangesMap Changes;

  ChangeSet() {}

  void PostChange(StateChange::Kind kind, FileInfo * entry,
		  FileInfo * ancestor);

  void CompareLocations(const Location * origin,
			const Location * ancestor);
  void CompareFiles(FileInfo * entry, FileInfo * ancestor);
};

} // namespace Attic

#endif // _CHANGESET_H
