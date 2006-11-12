#ifndef _CHANGESET_H
#define _CHANGESET_H

#include "MessageLog.h"
#include "StateChange.h"

#include <string>
#include <map>
#include <deque>

namespace Attic {

class Location;
class StateMap;
class ChangeSet
{
  void PostAddChange(FileInfo * entry);
  void PostRemoveChange(FileInfo * entryParent, const std::string& childName,
			FileInfo * ancestorChild);

  inline void PostUpdateChange(FileInfo * entry, FileInfo * ancestor) {
    PostChange(StateChange::Update, entry, ancestor);
  }

  inline void PostUpdatePropsChange(FileInfo * entry, FileInfo * ancestor) {
    PostChange(StateChange::UpdateProps, entry, ancestor);
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

  void CompareStates(const Location * origin,
		     const StateMap * ancestor);
  void CompareStates(const StateMap * origin,
		     const StateMap * ancestor);
  void CompareFiles(FileInfo * entry, FileInfo * ancestor);
};

} // namespace Attic

#endif // _CHANGESET_H
