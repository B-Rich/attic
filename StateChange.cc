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
    out << "m ";
    break;
  default:
    assert(0);
    break;
  }

  out << Path::Combine(Item->Repository->Moniker, Item->FullName)
      << std::endl;
}

void StateChange::Execute(std::ostream& out, Location * targetLocation)
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
      out << "C ";
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
      File::Copy(Item->Pathname, targetPath);
      out << "U ";
    }
    else {
      assert(0);
    }
    break;

  case Remove:
    if (targetInfo.Exists()) {
      if (Item->IsDirectory())
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

} // namespace Attic
