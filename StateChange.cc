#include "StateChange.h"
#include "StateMap.h"
#include "Location.h"

namespace Attic {

void StateChange::Report(MessageLog& log) const
{
  std::string prefix;

  switch (ChangeKind) {
  case Add:
    prefix = "A ";
    break;
  case Remove:
    prefix = "R ";
    break;
  case Update:
    prefix = "M ";
    break;
  case UpdateProps:
    prefix = "p ";
    break;
  default:
    assert(0);
    break;
  }

  LOG(log, Message,
      prefix << Path::Combine(Item->Repository->Moniker, Item->FullName));
}

std::deque<FileInfo *> *
StateChange::ExistsAtLocation(StateMap * stateMap,
			      Location * targetLocation) const
{
  if (targetLocation->CurrentState)
    stateMap = targetLocation->CurrentState;

  return stateMap->FindDuplicate(Item);
}

void StateChange::Execute(MessageLog& log, Location * targetLocation,
			  const StateChangesMap * changesMap)
{
  Path	      targetPath(Path::Combine(targetLocation->CurrentPath,
				       Item->FullName));
  FileInfo    targetInfo(targetPath);
  std::string label;

  switch (ChangeKind) {
  case Add:
    if (Item->IsDirectory()) {
      if (targetInfo.Exists()) {
	if (targetInfo.IsDirectory())
	  return;
	targetInfo.Delete();
	LOG(log, Message, "D " << targetPath);
      }
      Directory::CreateDirectory(targetPath);
      label = "c ";
    }
    else if (Item->IsRegularFile()) {
      if (targetInfo.Exists()) {
	if (targetInfo.IsRegularFile() &&
	    targetInfo.Checksum() == Item->Checksum()) {
	  StateChangesMap changes;
	  CompareFiles(Item, &targetInfo, changes);
	  if (! changes.empty()) {
	    Item->CopyAttributes(targetPath);
	    label = "p ";
	    break;
	  }
	  return;
	}
	targetInfo.Delete();
	LOG(log, Message, "D " << targetPath);
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
	  label = "m ";
	  break;
	} else {
	  // jww (2006-11-09): Do this through the broker, since it
	  // must happen remotely
	  Directory::CreateDirectory(targetPath.DirectoryName());
	  File::Copy(Path::Combine(targetLocation->CurrentPath, Duplicate->FullName),
		     targetPath);
	  label = "u ";
	  break;
	}
      }

      File::Copy(Item->Pathname, targetPath);
      label = "U ";
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
    label = "D ";
    break;

  case Update:
    if (Item->IsRegularFile())
      File::Copy(Item->Pathname, targetPath);
    else
      assert(0);
    label = "P ";
    break;

  case UpdateProps:
    Item->CopyAttributes(targetPath);
    label = "p ";
    break;

  default:
    assert(0);
    break;
  }

  LOG(log, Message,
      label << Path::Combine(targetLocation->Moniker, Item->FullName));
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

void StateChange::DebugPrint(MessageLog& log) const
{
  std::string label;

  switch (ChangeKind) {
  case Nothing:	    label = "Nothing "; break;
  case Add:	    label = "Add "; break;
  case Remove:	    label = "Remove "; break;
  case Update:	    label = "Update "; break;
  case UpdateProps: label = "UpdateProps "; break;
  default:          assert(0); break;
  }

  if (Ancestor) {
    LOG(log, Message,
	label << "Ancestor(" << Ancestor << ") " <<
	Item->FullName << " ");
  } else {
    LOG(log, Message, label << Item->FullName << " ");
  }
}

int StateChange::Depth() const
{
  assert(Ancestor);
  return 0;
  //return Ancestor->Depth + (ChangeKind == Add ? 1 : 0);
}

void PostChange(FileInfo * entry, FileInfo * ancestor,
		StateChange::Kind kind, StateChangesMap& changesMap)
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

void PostAddChange(FileInfo * entry, StateChangesMap& changesMap)
{
  if (entry->IsDirectory()) {
    PostChange(entry, NULL, StateChange::Add, changesMap);

    for (FileInfo::ChildrenMap::const_iterator i = entry->ChildrenBegin();
	 i != entry->ChildrenEnd();
	 i++)
      PostAddChange((*i).second, changesMap);
  } else {
    PostChange(entry, NULL, StateChange::Add, changesMap);
  }
}

void PostRemoveChange(FileInfo * parent, const std::string& childName,
		      FileInfo * ancestorChild, StateChangesMap& changesMap)
{
  FileInfo * missingChild =
    new FileInfo(Path::Combine(parent->FullName, childName), parent,
		 parent->Repository);

  if (ancestorChild->IsDirectory())
    for (FileInfo::ChildrenMap::const_iterator
	   i = ancestorChild->ChildrenBegin();
	 i != ancestorChild->ChildrenEnd();
	 i++)
      PostRemoveChange(missingChild, (*i).first, (*i).second, changesMap);

  PostChange(missingChild, ancestorChild, StateChange::Remove, changesMap);
}

void CompareFiles(FileInfo * entry, FileInfo * ancestor,
		  StateChangesMap& changesMap)
{
  assert(entry->Repository);

  if (! ancestor) {
    PostAddChange(entry, changesMap);
    return;
  }

  if (! entry->IsVirtual() && entry->Name != ancestor->Name)
    throw Exception("Names do not match in comparison: " +
		    entry->FullName + " != " + ancestor->Name);

  bool updateRegistered = false;

  if (entry->FileKind() != ancestor->FileKind()) {
    PostUpdateChange(entry, ancestor, changesMap);
    updateRegistered = true;
  }
  else if (entry->IsRegularFile()) {
    if (entry->Length() != ancestor->Length()) {
      PostUpdateChange(entry, ancestor, changesMap);
      updateRegistered = true;
    }
    else if (! entry->Repository->TrustLengthOnly) {
      if (entry->LastWriteTime() != ancestor->LastWriteTime() ||
	  (! entry->Repository->TrustTimestamps &&
	   entry->Checksum() != ancestor->Checksum())) {
	PostUpdateChange(entry, ancestor, changesMap);
	updateRegistered = true;
      }
    }
  }
  else if (entry->Exists() &&
	   entry->LastWriteTime() != ancestor->LastWriteTime()) {
    PostUpdatePropsChange(entry, ancestor, changesMap);
    updateRegistered = true;
  }

  if (! updateRegistered && entry->Exists() &&
      (entry->Permissions() != ancestor->Permissions() ||
       entry->OwnerId()     != ancestor->OwnerId()	||
       entry->GroupId()     != ancestor->GroupId()))
    PostUpdatePropsChange(entry, ancestor, changesMap);

  if (! entry->IsDirectory())
    return;

  bool updateProps = false;

  for (FileInfo::ChildrenMap::iterator i = entry->ChildrenBegin();
       i != entry->ChildrenEnd();
       i++) {
    FileInfo * ancestorChild = ancestor->FindChild((*i).first);
    if (ancestorChild != NULL) {
      ancestorChild->SetFlags(FILEINFO_HANDLED);
      CompareFiles((*i).second, ancestorChild, changesMap);
    } else {
      PostAddChange((*i).second, changesMap);
      updateProps = true;
    }
  }

  for (FileInfo::ChildrenMap::iterator i = ancestor->ChildrenBegin();
       i != ancestor->ChildrenEnd();
       i++) {
    if ((*i).second->HasFlags(FILEINFO_HANDLED)) {
      (*i).second->ClearFlags(FILEINFO_HANDLED);
    }
    else if (entry->FindChild((*i).first) == NULL) {
      PostRemoveChange(entry, (*i).first, (*i).second, changesMap);
      updateProps = true;
    }
  }

  if (updateProps && ! updateRegistered)
    PostUpdatePropsChange(entry, ancestor, changesMap);
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
