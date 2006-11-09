#include "StateEntry.h"
#include "StateMap.h"
#include "binary.h"

namespace Attic {

void StateEntry::Copy(const std::string& target)
{
  switch (Info.FileKind()) {
  case FileInfo::File: {
    FileInfo info(target);
    if (info.Exists())
      info.Delete();

    // Copy the entry, and keep the file attributes identical
    File::Copy(Info.FullName, target);
    Info.CopyAttributes(target);
    break;
  }

  case FileInfo::Directory: {
    CopyDirectory(target, true);
    break;
  }

  default:
    assert(false);
    break;
  }
}

void StateEntry::CopyDirectory(const std::string& target, bool andChildren)
{
  Directory::CreateDirectory(target);

  if (andChildren)
    for (ChildrenMap::iterator i = Children.begin();
	 i != Children.end();
	 i++)
      (*i).second->Copy(Path::Combine(target, (*i).second->Info.RelativeName));

  Info.CopyAttributes(target);
}

void StateEntry::MarkDeletePending()
{
  DeletePending = true;

  for (ChildrenMap::iterator i = Children.begin();
       i != Children.end();
       i++)
    (*i).second->MarkDeletePending();
}

void StateEntry::Move(const std::string& target)
{
  switch (Info.FileKind()) {
  case FileInfo::Directory:
    Directory::Move(Info.FullName, target);
    break;

  default:
    File::Move(Info.FullName, target);
    break;
  }

  Info.CopyAttributes(target);
}

void StateEntry::Report() const
{
  std::cout << Info.FullName << std::endl;

  for (ChildrenMap::const_iterator i = Children.begin();
       i != Children.end();
       i++)
    (*i).second->Report();
}

bool StateEntry::Copy(StateEntry * dirToCopyInto, bool moveOnly)
{
  std::string baseName   = Info.Name;
  std::string targetPath = Path::Combine(dirToCopyInto->Info.FullName, baseName);

  switch (Info.FileKind()) {
  case FileInfo::File: {
    StateEntry * other = dirToCopyInto->StateMapObj->FindDuplicate(this);
    if (other != NULL) {
      if (other->DeletePending && moveOnly) {
	other->StateMapObj->BackupEntry(other);
	other->Move(targetPath);

	other->DeletePending = false;
	StateEntry::ChildrenMap::iterator i =
	  other->Parent->Children.find(other->Info.Name);
	assert(i != other->Parent->Children.end());
	other->Parent->Children.erase(i);

	dirToCopyInto->FindOrCreateChild(baseName);
	dirToCopyInto->Info.Reset();
	return true;
      }
      else if (! moveOnly) {
	other->Copy(targetPath);
	dirToCopyInto->FindOrCreateChild(baseName);
	dirToCopyInto->Info.Reset();
	return true;
      }
    }
    else if (! moveOnly) {
      Copy(targetPath);
      dirToCopyInto->FindOrCreateChild(baseName);
      dirToCopyInto->Info.Reset();
      return true;
    }
    break;
  }

  case FileInfo::Directory: {
    if (moveOnly)
      break;

    CopyDirectory(targetPath, false);

    StateEntry * newDir = dirToCopyInto->FindOrCreateChild(baseName);
    for (ChildrenMap::iterator i = Children.begin();
	 i != Children.end();
	 i++)
      (*i).second->Copy(newDir, false);

    dirToCopyInto->Info.Reset();
    return true;
  }

  default:
    assert(false);
    break;
  }

  return false;
}

} // namespace Attic
