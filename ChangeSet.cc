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
    entryParent->Repository->SiteBroker->CreateFileInfo
      (Path::Combine(entryParent->FullName, childName), entryParent);

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
  else if (entry->Exists()) {
    if (entry->IsRegularFile()) {
      if (entry->Length() != ancestor->Length()) {
	PostUpdateChange(entry, ancestor);
	updateRegistered = true;
      }
      else if (! entry->Repository->TrustLengthOnly &&
	       (entry->LastWriteTime() != ancestor->LastWriteTime() ||
		(entry->Repository->UseChecksums &&
		 entry->Checksum() != ancestor->Checksum()))) {
	PostUpdateChange(entry, ancestor);
	updateRegistered = true;
      }
    }

    if (! updateRegistered && ! entry->CompareAttributes(*ancestor)) {
      PostUpdateAttrsChange(entry, ancestor);
      updateRegistered = true;
    }
  }

  if (! entry->IsDirectory())
    return;

  bool updateAttrs = false;

  for (FileInfo::ChildrenMap::iterator i = entry->ChildrenBegin();
       i != entry->ChildrenEnd();
       i++) {
    FileInfo * ancestorChild = ancestor->FindChild((*i).first);
    if (ancestorChild != NULL) {
      ancestorChild->SetFlags(FILEINFO_HANDLED);
      CompareFiles((*i).second, ancestorChild);
    } else {
      PostAddChange((*i).second);
      updateAttrs = true;
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
      updateAttrs = true;
    }
  }

  if (updateAttrs && ! updateRegistered)
    PostUpdateAttrsChange(entry, ancestor);
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

  if (left->ChangeKind == StateChange::UpdateAttrs &&
      ! right->ChangeKind == StateChange::UpdateAttrs)
    return false;
  if (! left->ChangeKind == StateChange::UpdateAttrs
      && right->ChangeKind == StateChange::UpdateAttrs)
    return true;

  if (left->ChangeKind == StateChange::Remove)
    return right->Item->FullName < left->Item->FullName;
  else
    return left->Item->FullName < right->Item->FullName;
}

void ChangeSet::CompareLocations(const Location * origin,
				 const Location * ancestor)
{
  CompareFiles(origin->Root(), ancestor ? ancestor->Root() : NULL);
}

} // namespace Attic
