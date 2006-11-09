#ifndef _STATECHANGE_H
#define _STATECHANGE_H

#include <string>
#include <map>

namespace Attic {

class FileInfo;
class Location;
class StateMap;
class StateEntry;
class StateChange
{
public:
  enum Kind {
    Nothing,
    Add,			// in this case, Ancestor is the parent dir
    Remove,
    Update,
    UpdateProps
  };

  StateChange * Next;
  Kind		ChangeKind;
  FileInfo *	Item;
  FileInfo *	Ancestor;

  StateChange(Kind _ChangeKind, FileInfo * _Item, FileInfo * _Ancestor)
    : Next(NULL), ChangeKind(_ChangeKind),
      Item(_Item), Ancestor(_Ancestor) {}

  void Report(std::ostream& out) const;
  void Execute(std::ostream& out, Location * targetLocation);
  void Execute(StateMap * stateMap);
  void DebugPrint(std::ostream& out) const;
  int  Depth() const;
};

typedef std::map<std::string, StateChange *>  StateChangesMap;
typedef std::pair<std::string, StateChange *> StateChangesPair;

void PostChange(StateChangesMap& changesMap, StateChange::Kind kind,
		FileInfo * entry, FileInfo * ancestor);

struct StateChangeComparer
{
  bool operator()(const StateChange * left, const StateChange * right) const
  {
    bool leftIsAdd	    = left->ChangeKind == StateChange::Add;
    bool leftIsUpdateProps  = (! leftIsAdd &&
			       left->ChangeKind == StateChange::UpdateProps);
    bool rightIsAdd         = right->ChangeKind == StateChange::Add;
    bool rightIsUpdateProps = (! rightIsAdd &&
			       right->ChangeKind == StateChange::UpdateProps);

    if (leftIsAdd && ! rightIsAdd)
      return true;
    if (! leftIsAdd && rightIsAdd)
      return false;
    if (leftIsUpdateProps && ! rightIsUpdateProps)
      return false;
    if (! leftIsUpdateProps && rightIsUpdateProps)
      return true;

#if 0
    if (left->DatabaseOnly && ! right->DatabaseOnly)
      return false;
    if (! left->DatabaseOnly && right->DatabaseOnly)
      return true;
#endif

    return (right->Depth() - left->Depth()) < 0;
  }
};

} // namespace Attic

#endif // _STATECHANGE_H
