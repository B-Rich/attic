#include "ChangeSet.h"
#include "FileInfo.h"
#include "Location.h"

namespace Attic {

void ChangeSet::PostChange(StateChange::Kind kind,
			   FileInfo * entry, FileInfo * ancestor)
{
  StateChange * newChange = new StateChange(kind, entry, ancestor);

  ChangesMap::iterator i = Changes.find(entry->FullName);
  if (i == Changes.end()) {
    std::pair<ChangesMap::iterator, bool> result =
      Changes.insert(ChangesPair(entry->FullName, newChange));
    assert(result.second);
  } else {
    newChange->Next = (*i).second;
    (*i).second = newChange;
  }
}

void ChangeSet::PostAddChange(FileInfo * entry)
{
  if (entry->IsDirectory()) {
    PostChange(StateChange::Add, entry, NULL);

    for (FileInfo::ChildrenMap::const_iterator i = entry->ChildrenBegin();
	 i != entry->ChildrenEnd();
	 i++)
      PostAddChange((*i).second);
  } else {
    PostChange(StateChange::Add, entry, NULL);
  }
}

void ChangeSet::PostRemoveChange(FileInfo *	    entryParent,
				 const std::string& childName,
				 FileInfo *	    ancestorChild)
{
  FileInfo * missingChild =
    new FileInfo(Path::Combine(entryParent->FullName, childName),
		 entryParent, entryParent->Repository);

  if (ancestorChild->IsDirectory())
    for (FileInfo::ChildrenMap::const_iterator
	   i = ancestorChild->ChildrenBegin();
	 i != ancestorChild->ChildrenEnd();
	 i++)
      PostRemoveChange(missingChild, (*i).first, (*i).second);

  PostChange(StateChange::Remove, missingChild, ancestorChild);
}

void ChangeSet::CompareFiles(FileInfo * entry, FileInfo * ancestor)
{
  assert(entry->Repository);

  if (! ancestor) {
    PostAddChange(entry);
    return;
  }

  if (! entry->IsVirtual() && entry->Name != ancestor->Name)
    throw Exception("Names do not match in comparison: " +
		    entry->FullName + " != " + ancestor->Name);

  bool updateRegistered = false;

  if (entry->FileKind() != ancestor->FileKind()) {
    PostUpdateChange(entry, ancestor);
    updateRegistered = true;
  }
  else if (entry->IsRegularFile()) {
    if (entry->Length() != ancestor->Length()) {
      PostUpdateChange(entry, ancestor);
      updateRegistered = true;
    }
    else if (! entry->Repository->TrustLengthOnly) {
      if (entry->LastWriteTime() != ancestor->LastWriteTime() ||
	  (! entry->Repository->TrustTimestamps &&
	   entry->Checksum() != ancestor->Checksum())) {
	PostUpdateChange(entry, ancestor);
	updateRegistered = true;
      }
    }
  }
  else if (entry->Exists() &&
	   entry->LastWriteTime() != ancestor->LastWriteTime()) {
    PostUpdatePropsChange(entry, ancestor);
    updateRegistered = true;
  }

  if (! updateRegistered && entry->Exists() &&
      (entry->Permissions() != ancestor->Permissions() ||
       entry->OwnerId()     != ancestor->OwnerId()	||
       entry->GroupId()     != ancestor->GroupId()))
    PostUpdatePropsChange(entry, ancestor);

  if (! entry->IsDirectory())
    return;

  bool updateProps = false;

  for (FileInfo::ChildrenMap::iterator i = entry->ChildrenBegin();
       i != entry->ChildrenEnd();
       i++) {
    FileInfo * ancestorChild = ancestor->FindChild((*i).first);
    if (ancestorChild != NULL) {
      ancestorChild->SetFlags(FILEINFO_HANDLED);
      CompareFiles((*i).second, ancestorChild);
    } else {
      PostAddChange((*i).second);
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
      PostRemoveChange(entry, (*i).first, (*i).second);
      updateProps = true;
    }
  }

  if (updateProps && ! updateRegistered)
    PostUpdatePropsChange(entry, ancestor);
}

bool ChangeSet::ChangeComparer::operator()
  (const StateChange * left, const StateChange * right) const
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

void ChangeSet::CompareStates(const Location * origin,
			      const StateMap * ancestor)
{
  CompareFiles(origin->Root(), ancestor->Root);
}

void ChangeSet::CompareStates(const StateMap * origin,
			      const StateMap * ancestor)
{
  CompareFiles(origin->Root, ancestor->Root);
}

} // namespace Attic
