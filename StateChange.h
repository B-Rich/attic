#ifndef _STATECHANGE_H
#define _STATECHANGE_H

#include <string>
#include <map>
#include <deque>

namespace Attic {

class StateChange;

typedef std::map<std::string, StateChange *>  StateChangesMap;
typedef std::pair<std::string, StateChange *> StateChangesPair;

class FileInfo;
class Location;
class StateMap;
class StateEntry;
class StateChange
{
public:
  enum Kind {
    Nothing, Add, Remove, Update, UpdateProps
  };

  StateChange * Next;
  Kind		ChangeKind;
  FileInfo *	Item;

  union {
    FileInfo * Ancestor;
    std::deque<FileInfo *> * Duplicates;
  };

  StateChange(Kind _ChangeKind, FileInfo * _Item, FileInfo * _Ancestor)
    : Next(NULL), ChangeKind(_ChangeKind),
      Item(_Item), Ancestor(_Ancestor) {}

  void Report(std::ostream& out) const;
  void Execute(std::ostream& out, Location * targetLocation,
	       const StateChangesMap * changesMap);
  void Execute(StateMap * stateMap);
  void DebugPrint(std::ostream& out) const;
  int  Depth() const;

  std::deque<FileInfo *> *
  ExistsAtLocation(StateMap * stateMap, Location * targetLocation) const;
};

void PostChange(StateChangesMap& changesMap, StateChange::Kind kind,
		FileInfo * entry, FileInfo * ancestor);

struct StateChangeComparer {
  bool operator()(const StateChange * left, const StateChange * right) const;
};

typedef std::deque<StateChange *> StateChangesArray;

} // namespace Attic

#endif // _STATECHANGE_H
