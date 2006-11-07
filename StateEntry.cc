#include "StateEntry.h"
#include "StateMap.h"
#include "binary.h"

#define BINARY_VERSION 0x00000001L

namespace Attic {

bool StateEntry::TrustMode = true;

StateEntry * StateEntry::FindChild(const std::string& name)
{
  for (children_map::iterator i = Children.begin();
       i != Children.end();
       i++)
    if ((*i).second->Info.Name == name)
      return (*i).second;

  return NULL;
}

void StateEntry::WriteTo(std::ostream& w, bool top)
{
  FileInfo::Kind fileKind = Info.FileKind();

  write_binary_long(w, static_cast<int>(fileKind));

  if (fileKind != FileInfo::Collection) {
    write_binary_string(w, top ? Info.FullName : Info.Name);
    top = false;

    if (fileKind != FileInfo::Nonexistant) {
      write_binary_number(w, Info.LastAccessTime());
      write_binary_number(w, Info.LastWriteTime());
      write_binary_number(w, Info.Permissions());
      write_binary_number(w, Info.OwnerId());
      write_binary_number(w, Info.GroupId());

      if (fileKind == FileInfo::File) {
	write_binary_long(w, Info.Length());
	write_binary_number(w, Info.Checksum());
      }
    }
  } else {
    write_binary_long(w, BINARY_VERSION);
  }

  write_binary_long(w, (int)Children.size());
  for (children_map::iterator i = Children.begin();
       i != Children.end();
       i++)
    (*i).second->WriteTo(w, top);
}

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
    for (children_map::iterator i = Children.begin();
	 i != Children.end();
	 i++)
      (*i).second->Copy(Path::Combine(target, (*i).second->Info.RelativeName));

  Info.CopyAttributes(target);
}

void StateEntry::MarkDeletePending()
{
  DeletePending = true;

  for (children_map::iterator i = Children.begin();
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

  for (children_map::const_iterator i = Children.begin();
       i != Children.end();
       i++)
    (*i).second->Report();
}

StateEntry *
StateEntry::LoadElement(StateMap *	   StateMapObj,
			StateEntry *	   Parent,
			const std::string& parentPath,
			const std::string& relativePath,
			char *&		   data)
{
  int kind;
  read_binary_long(data, kind);

  FileInfo info;

  info.fileKind = static_cast<FileInfo::Kind>(kind);
  assert(info.fileKind < FileInfo::Last);

  if (kind != FileInfo::Collection) {
    read_binary_string(data, info.Name);
    assert(! info.Name.empty());

    if (info.Name[0] == '/') {
      info.FullName     = info.Name;
      info.Name		= Path::GetFileName(info.FullName);
      info.RelativeName = info.Name;
    } else {
      info.FullName     = Path::Combine(parentPath, info.Name);
      info.RelativeName = Path::Combine(relativePath, info.Name);
    }

    if (kind != FileInfo::Nonexistant) {
      info.didstat = true;

      read_binary_number(data, info.LastAccessTime());
      read_binary_number(data, info.LastWriteTime());
      mode_t mode;
      read_binary_number(data, mode);
      info.SetPermissions(mode);
      read_binary_number(data, info.OwnerId());
      read_binary_number(data, info.GroupId());

      if (kind == FileInfo::File) {
	read_binary_long(data, info.Length());

	info.readcsum = true;
	read_binary_number(data, info.Checksum());
      }
    }
  } else {
    if (read_binary_long<long>(data) != BINARY_VERSION)
      return NULL;
  }

  StateEntry * entry = new StateEntry(StateMapObj, Parent, info);
  if (kind == FileInfo::File)
    StateMapObj->ChecksumDict[info.Checksum()] = entry;

  int children = read_binary_long<int>(data);
  for (int i = 0; i < children; i++)
    LoadElement(StateMapObj, entry, entry->Info.FullName,
		entry->Info.RelativeName, data);

  return entry;
}

StateEntry * StateEntry::CreateChild(const std::string& name)
{
  return new StateEntry(StateMapObj, this,
			FileInfo(Path::Combine(Info.FullName, name),
				 Path::Combine(Info.RelativeName, name)));
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
	StateEntry::children_map::iterator i =
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
    for (children_map::iterator i = Children.begin();
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

void StateEntry::CompareTo(StateEntry * other, StateEntry * target)
{
  std::string msg;

  FileInfo::Kind fileKind = Info.FileKind();

  if (fileKind != FileInfo::Collection &&
      Info.Name != other->Info.Name)
    throw Exception("Names do not match in comparison: " +
		    Info.FullName + " != " + other->Info.Name);

  bool updateRegistered = false;

  StateEntry * goal = target ? target : this;

  if (fileKind != other->Info.FileKind()) {
    if (fileKind != FileInfo::Nonexistant)
      goal->StateMapObj->RegisterDelete(this, goal);
    goal->StateMapObj->RegisterCopy(other, Parent, goal->Parent);
    return;
  }
  else if (fileKind == FileInfo::File) {
    if (Info.Length() != other->Info.Length()) {
      goal->StateMapObj->RegisterUpdate(this, goal, other, "length");
      updateRegistered = true;
    }
    else if (Info.LastWriteTime() != other->Info.LastWriteTime() ||
	     (! TrustMode && Info.Checksum() != other->Info.Checksum())) {
      goal->StateMapObj->RegisterUpdate(this, goal, other, "contents");
      updateRegistered = true;
    }
    else if (Info.Permissions()	!= other->Info.Permissions() ||
	     Info.OwnerId()	!= other->Info.OwnerId() ||
	     Info.GroupId()	!= other->Info.GroupId()) {
      goal->StateMapObj->RegisterUpdateProps(this, goal, this != goal ? goal : other,
					      "attributes");
      updateRegistered = true;
    }
  }
  else if (fileKind != FileInfo::Collection &&
	   fileKind != FileInfo::Nonexistant &&
	   (Info.LastWriteTime() != other->Info.LastWriteTime() ||
	    Info.Permissions()	 != other->Info.Permissions() ||
	    Info.OwnerId()	 != other->Info.OwnerId() ||
	    Info.GroupId()	 != other->Info.GroupId())) {
    goal->StateMapObj->RegisterUpdateProps(this, goal, this != goal ? goal : other,
					    "modtime changed");
    updateRegistered = true;
  }

  bool updateProps = false;

  for (children_map::iterator i = Children.begin();
       i != Children.end();
       i++) {
    StateEntry * otherChild = other->FindChild((*i).first);
    if (otherChild != NULL) {
      otherChild->Handled = true;

      StateEntry * goalChild  = target ? goal->FindChild((*i).first) : NULL;
      if (! target || goalChild)
	(*i).second->CompareTo(otherChild, goalChild);
    }
    else if (! target) {
      StateMapObj->RegisterDelete((*i).second, (*i).second);
      updateProps = true;
    }
  }

  for (children_map::iterator i = other->Children.begin();
       i != other->Children.end();
       i++) {
    if ((*i).second->Handled) {
      (*i).second->Handled = false;
    }
    else if (FindChild((*i).first) == NULL) {
      goal->StateMapObj->RegisterCopy((*i).second, this, goal);
      updateProps = true;
    }
  }

  if (updateProps && ! updateRegistered)
    goal->StateMapObj->RegisterUpdateProps(this, goal, other);
}

} // namespace Attic
