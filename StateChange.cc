#include "StateChange.h"
#include "StateMap.h"
#include "Location.h"

#include <iostream>

namespace Attic {

void StateChange::Report(std::ostream& out) const
{
  switch (ChangeKind) {
  case Add:
    out << "A ";
    break;
  case Remove:
    out << "R ";
    break;
  case Update:
    out << "M ";
    break;
  case UpdateProps:
    out << "p ";
    break;
  default:
    assert(0);
    break;
  }

  out << Path::Combine(Item->Repository->Moniker, Item->FullName)
      << std::endl;
}

std::deque<FileInfo *> *
StateChange::ExistsAtLocation(StateMap * stateMap,
			      Location * targetLocation) const
{
  if (targetLocation->CurrentState)
    stateMap = targetLocation->CurrentState;

  return stateMap->FindDuplicate(Item);
}

void StateChange::Execute(std::ostream& out, Location * targetLocation,
			  const StateChangesMap * changesMap)
{
  Path targetPath(Path::Combine(targetLocation->CurrentPath, Item->FullName));

  FileInfo targetInfo(targetPath);

  switch (ChangeKind) {
  case Add:
    if (Item->IsDirectory()) {
      if (targetInfo.Exists()) {
	if (targetInfo.IsDirectory())
	  return;
	targetInfo.Delete();
	out << "D " << targetPath << std::endl;
      }
      Directory::CreateDirectory(targetPath);
      out << "c ";
    }
    else if (Item->IsRegularFile()) {
      if (targetInfo.Exists()) {
	if (targetInfo.IsRegularFile() &&
	    targetInfo.Checksum() == Item->Checksum()) {
	  StateChangesMap changes;
	  Item->CompareTo(&targetInfo, changes);
	  if (! changes.empty()) {
	    Item->CopyAttributes(targetPath);
	    out << "p ";
	    break;
	  }
	  return;
	}
	targetInfo.Delete();
	out << "D " << targetPath << std::endl;
      }

      // If Duplicate is non-NULL (and this applies only for Add
      // changes), then an exact copy of our entry already exists in
      // the target location.  If it is marked for deletion, we'll
      // move it to the target path; if it's not, we'll copy it over
      // there.
      if (Duplicates) {
	FileInfo * Duplicate = Duplicates->front();

	bool markedForDeletion = false;
	if (changesMap)
	  for (std::deque<FileInfo *>::const_iterator i = Duplicates->begin();
	       i != Duplicates->end();
	       i++) {
	    StateChangesMap::const_iterator j = changesMap->find((*i)->FullName);
	    if (j != changesMap->end()) {
	      for (StateChange * ptr = (*j).second; ptr; ptr = ptr->Next)
		if (ptr->ChangeKind == Remove) {
		  markedForDeletion = true;
		  Duplicate = ptr->Item;
		  break;
		}
	    }
	    if (markedForDeletion)
	      break;
	  }

	if (markedForDeletion) {
	  Directory::CreateDirectory(targetPath.DirectoryName());
	  File::Move(Path::Combine(targetLocation->CurrentPath, Duplicate->FullName),
		     targetPath);
	  out << "m ";
	  break;
	} else {
	  // jww (2006-11-09): Do this through the broker, since it
	  // must happen remotely
	  Directory::CreateDirectory(targetPath.DirectoryName());
	  File::Copy(Path::Combine(targetLocation->CurrentPath, Duplicate->FullName),
		     targetPath);
	  out << "u ";
	  break;
	}
      }

      File::Copy(Item->Pathname, targetPath);
      out << "U ";
    }
    else {
      assert(0);
    }
    break;

  case Remove:
    if (targetInfo.Exists()) {
      if (targetInfo.IsDirectory())
	Directory::Delete(targetPath);
      else
	File::Delete(targetPath);
    }
    out << "D ";
    break;

  case Update:
    if (Item->IsRegularFile())
      File::Copy(Item->Pathname, targetPath);
    else
      assert(0);
    out << "P ";
    break;

  case UpdateProps:
    Item->CopyAttributes(targetPath);
    out << "p ";
    break;

  default:
    assert(0);
    break;
  }

  out << Path::Combine(targetLocation->Moniker, Item->FullName)
      << std::endl;
}

void StateChange::Execute(StateMap * stateMap)
{
  FileInfo * entry;

  switch (ChangeKind) {
  case Add:
    entry = stateMap->Root->FindOrCreateMember(Item->FullName);
    Item->CopyDetails(*entry, true);
    Item->CopyAttributes(*entry, true);
    break;

  case Remove:
    entry = stateMap->Root->FindMember(Item->FullName);
    if (entry)
      entry->Parent->DestroyChild(entry);
    break;

  case Update:
    entry = stateMap->Root->FindMember(Item->FullName);
    if (entry)
      Item->CopyDetails(*entry, true);
    break;

  case UpdateProps:
    entry = stateMap->Root->FindMember(Item->FullName);
    if (entry)
      Item->CopyAttributes(*entry, true);
    break;

  default:
    assert(0);
    break;
  }
}

void StateChange::DebugPrint(std::ostream& out) const
{
  switch (ChangeKind) {
  case Nothing: out << "Nothing "; break;
  case Add: out << "Add "; break;
  case Remove: out << "Remove "; break;
  case Update: out << "Update "; break;
  case UpdateProps: out << "UpdateProps "; break;
  }

  if (Ancestor)
    out << "Ancestor(" << Ancestor << ") ";

  out << Item->FullName << " ";
}

int StateChange::Depth() const
{
  assert(Ancestor);
  return 0;
  //return Ancestor->Depth + (ChangeKind == Add ? 1 : 0);
}

void PostChange(StateChangesMap& changesMap, StateChange::Kind kind,
		FileInfo * entry, FileInfo * ancestor)
{
  StateChange * newChange = new StateChange(kind, entry, ancestor);

  StateChangesMap::iterator i = changesMap.find(entry->FullName);
  if (i == changesMap.end()) {
    std::pair<StateChangesMap::iterator, bool> result =
      changesMap.insert(StateChangesPair(entry->FullName, newChange));
    assert(result.second);
  } else {
    newChange->Next = (*i).second;
    (*i).second = newChange;
  }
}

bool StateChangeComparer::operator()(const StateChange * left,
				     const StateChange * right) const
{
  if (left->ChangeKind == StateChange::Remove &&
      ! right->ChangeKind == StateChange::Remove)
    return true;
  if (! left->ChangeKind == StateChange::Remove &&
      right->ChangeKind == StateChange::Remove)
    return false;

  if (left->ChangeKind == StateChange::UpdateProps &&
      ! right->ChangeKind == StateChange::UpdateProps)
    return false;
  if (! left->ChangeKind == StateChange::UpdateProps
      && right->ChangeKind == StateChange::UpdateProps)
    return true;

  if (left->ChangeKind == StateChange::Remove)
    return right->Item->FullName < left->Item->FullName;
  else
    return left->Item->FullName < right->Item->FullName;
}

} // namespace Attic
