#include "FileInfo.h"
#include "Location.h"
#include "binary.h"

#include <fstream>

#include <dirent.h>
#include <errno.h>
#include <cstdlib>
#include <cstring>
#if defined(HAVE_GETPWUID) || defined(HAVE_GETPWNAM)
#include <pwd.h>
#endif
#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "md5.h"

namespace Attic {

md5sum_t md5sum_t::checksum(const Path& path, md5sum_t& csum)
{
  md5_state_t state;
  md5_init(&state);

  char cbuf[8192];

  std::fstream fin(path.c_str());
  assert(fin.good());

  fin.read(cbuf, 8192);
  int read = fin.gcount();
  while (read > 0) {
    md5_append(&state, (md5_byte_t *)cbuf, read);
    if (fin.eof() || fin.bad())
      break;
    fin.read(cbuf, 8192);
    read = fin.gcount();
  }
  fin.close();

  md5_finish(&state, csum.digest);
}

std::ostream& operator<<(std::ostream& out, const md5sum_t& md5) {
  for (int i = 0; i < 16; i++) {
    out.fill('0');
    out << std::hex << (int)md5.digest[i];
  }
  return out;
}

void FileInfo::dostat() const
{
  Repository->SiteBroker->ReadFileProperties(FullName,
					    const_cast<FileInfo *>(this));
}

FileInfo::~FileInfo()
{
  if (Children) {
    for (ChildrenMap::iterator i = Children->begin();
	 i != Children->end();
	 i++)
      delete (*i).second;
  }

  if (IsTempFile())
    Delete();
}

void FileInfo::SetPath(const Path& _FullName)
{
  FullName = _FullName;
  Name     = Path::GetFileName(FullName);

#if 0
  if (Repository)
    Pathname = Path::Combine(Repository->SiteBroker->CurrentPath, FullName);
  else
#endif
    Pathname = FullName;
}

void FileInfo::GetFileInfos(ChildrenMap& store) const
{
  DIR * dirp = opendir(Pathname.c_str());
  if (dirp == NULL)
    return;

  struct dirent * dp;
  while ((dp = readdir(dirp)) != NULL) {
#ifdef DIRENT_HAS_D_NAMLEN
    int len = dp->d_namlen;
#else
    int len = std::strlen(dp->d_name);
#endif
    if (dp->d_name[0] == '.' &&
	(len == 1 || (len == 2 && dp->d_name[1] == '.')))
      continue;

    // This gets added to the parent upon construction
    new FileInfo(Path::Combine(FullName, dp->d_name),
		 const_cast<FileInfo *>(this), Repository);
  }

  (void)closedir(dirp);
}

int FileInfo::ChildrenSize() const
{
  if (! IsDirectory())
    throw Exception("Attempt to call ChildrenSize on a non-directory");

  if (! Children) {
    Children = new ChildrenMap;
    GetFileInfos(*Children);
  }
  return Children->size();
}

FileInfo::ChildrenMap::iterator FileInfo::ChildrenBegin() const
{
  if (! IsDirectory())
    throw Exception("Attempt to call ChildrenBegin on a non-directory");

  if (! Children) {
    Children = new ChildrenMap;
    GetFileInfos(*Children);
  }
  return Children->begin();
}

FileInfo * FileInfo::FindChild(const std::string& name)
{
  assert(! name.empty());

  if (! Children)
    return NULL;

  for (ChildrenMap::iterator i = ChildrenBegin();
       i != ChildrenEnd();
       i++)
    if ((*i).first == name)
      return (*i).second;

  return NULL;
}

void FileInfo::AddChild(FileInfo * entry)
{
  if (! Children)
    Children = new ChildrenMap;

  std::pair<ChildrenMap::iterator, bool> result =
    Children->insert(ChildrenPair(entry->Name, entry));
  assert(result.second);
}

FileInfo * FileInfo::CreateChild(const std::string& name)
{
  assert(! name.empty());

  return new FileInfo(Path::Combine(FullName, name), this, Repository);
}

void FileInfo::DestroyChild(FileInfo * child)
{
  if (Children)
    for (ChildrenMap::iterator i = Children->begin();
	 i != Children->end();
	 i++)
      if ((*i).second == child) {
	Children->erase(i);
	return;
      }
}

FileInfo * FileInfo::FindMember(const Path& path)
{
  if (path.empty())
    return this;

  FileInfo * current = this;

  std::string::size_type previ = 0;
  for (std::string::size_type index = path.find('/', previ);
       index != std::string::npos;
       previ = index + 1, index = path.find('/', previ)) {
    std::string part(path, previ, index - previ);
    current = current->FindChild(part);
    if (! current)
      return NULL;
  }

  return current->FindChild(std::string(path, previ));
}

FileInfo * FileInfo::FindOrCreateMember(const Path& path)
{
  if (path.empty())
    return this;

  FileInfo * current = this;

  std::string::size_type previ = 0;
  for (std::string::size_type index = path.find('/', previ);
       index != std::string::npos;
       previ = index + 1, index = path.find('/', previ)) {
    std::string part(path, previ, index - previ);
    current = current->FindOrCreateChild(part);
  }

  return current->FindOrCreateChild(std::string(path, previ));
}

void FileInfo::CopyDetails(FileInfo& dest, bool dataOnly) const
{
  dest.flags	    = flags;
  dest.info.st_mode = info.st_mode;

  dest.SetLength(Length());

  if (IsRegularFile() &&
      (dataOnly || flags & FILEINFO_READCSUM))
    dest.SetChecksum(Checksum());
}

void FileInfo::CopyAttributes(FileInfo& dest, bool dataOnly) const
{
  dest.SetFlags(FILEINFO_DIDSTAT);

  if (! dataOnly) {
    File::SetPermissions(dest.Pathname, Permissions());
    File::SetOwnership(dest.Pathname, OwnerId(), GroupId());
  }

  dest.SetPermissions(Permissions());

  dest.SetOwnerId(OwnerId());
  dest.SetGroupId(GroupId());

  if (! dataOnly)
    File::SetAccessTimes(dest.Pathname, LastAccessTime(), LastWriteTime());

  dest.SetLastAccessTime(LastAccessTime());
  dest.SetLastWriteTime(LastWriteTime());
}

void FileInfo::CopyAttributes(const Path& dest) const
{
  File::SetPermissions(dest, Permissions());
  File::SetOwnership(dest, OwnerId(), GroupId());
  File::SetAccessTimes(dest, LastAccessTime(), LastWriteTime());
}

const md5sum_t& FileInfo::Checksum() const
{
#if 0
  if (! IsRegularFile() || ! Exists())
    throw Exception("Attempt to calc checksum of non-file '" + Pathname + "'");
#endif
  if (! (flags & FILEINFO_READCSUM)) {
    md5sum_t::checksum(Pathname, csum);
    flags |= FILEINFO_READCSUM;
  }
  return csum;
}

md5sum_t FileInfo::CurrentChecksum() const
{
  if (! IsRegularFile())
    throw Exception("Attempt to calc checksum of non-file '" + Pathname + "'");

  md5sum_t sum;
  md5sum_t::checksum(Pathname, sum);
  return sum;
}

FileInfo FileInfo::Directory() const
{
  return FileInfo(FullName.DirectoryName(), Parent ? Parent->Parent : NULL,
		  Repository);
}

FileInfo FileInfo::LinkReference() const
{
  if (! IsSymbolicLink())
    throw Exception("Attempt to dereference non-symbol link '" + Pathname + "'");

  char buf[8192];
  if (readlink(Pathname.c_str(), buf, 8191) != -1)
    return FileInfo(Path(buf));
  else
    throw Exception("Failed to read symbol link '" + Pathname + "'");
}

void FileInfo::CreateDirectory()
{
  int index = Pathname.rfind('/');
  if (index != std::string::npos)
    Directory::CreateDirectory(std::string(Pathname, 0, index));
}

void FileInfo::Delete()
{
  if (IsDirectory()) {
    for (ChildrenMap::iterator i = ChildrenBegin();
	 i != ChildrenEnd();
	 i++)
      (*i).second->Delete();
    Directory::Delete(FullName);
  }
  else if (Exists() && ! IsVirtual()) {
    File::Delete(FullName);
  }
  ClearFlags(FILEINFO_EXISTS);
}

void FileInfo::Copy(const Path& dest) const
{
  if (IsRegularFile()) {
    File::Copy(Pathname, dest);
  }
  else if (IsDirectory()) {
    Directory::CreateDirectory(dest);

    for (ChildrenMap::iterator i = ChildrenBegin();
	 i != ChildrenEnd();
	 i++) {
      assert((*i).first == (*i).second->Name);
      (*i).second->Copy(Path::Combine(dest, (*i).first));
    }
  }
}

void FileInfo::Move(const Path& dest)
{
  File::Move(Pathname, dest);
}

FileInfo * FileInfo::ReadFrom(char *& data, FileInfo * parent,
			      Location * repository)
{
  FileInfo * entry = new FileInfo(repository);

  read_binary_string(data, entry->Name);
  read_binary_number(data, static_cast<FileInfoData&>(*entry));

  entry->Parent = parent;
  if (parent) {
    entry->SetPath(Path::Combine(parent->FullName, entry->Name));
    parent->AddChild(entry);
  } else {
    entry->SetPath(entry->Name);
  }

  if (entry->IsDirectory()) {
    int children = read_binary_long<int>(data);
    for (int i = 0; i < children; i++)
      ReadFrom(data, entry, repository);
  }
  return entry;
}

void FileInfo::DumpTo(std::ostream& out, bool verbose, int depth)
{
  for (int i = 0; i < depth; i++)
    out << "  ";

  if (verbose) {
    if (IsDirectory())
      out << "DIR: ";
    else if (IsRegularFile())
      out << "FIL: ";
    else if (IsSymbolicLink())
      out << "SYM: ";
    else if (Exists())
      out << "SPC: ";
    else
      out << "NON: ";
  }

  out << FullName << ':';

  if (IsRegularFile()) {
    out << " len " << Length();
    if (verbose)
      out << " csum " << Checksum();
  }
  if (Exists())
    out << " mod " << LastWriteTime();

  out << std::endl;

  if (Children)
    for (ChildrenMap::iterator i = Children->begin();
	 i != Children->end();
	 i++)
      (*i).second->DumpTo(out, verbose, depth + 1);
}

void FileInfo::WriteTo(std::ostream& out) const
{
  write_binary_string(out, Name);
  write_binary_number(out, static_cast<const FileInfoData&>(*this));

  if (IsDirectory()) {
    write_binary_long(out, (int)ChildrenSize());
    for (ChildrenMap::iterator i = ChildrenBegin();
	 i != ChildrenEnd();
	 i++)
      (*i).second->WriteTo(out);
  }
}

std::string FileInfo::Moniker() const
{
  assert(Repository);
  return Repository->SiteBroker->Moniker(FullName);
}

} // namespace Attic
