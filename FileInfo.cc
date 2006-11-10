#include <fstream>

#include <dirent.h>
#include <errno.h>
#include <cstdlib>
#define HAVE_GETPWUID
#define HAVE_GETPWNAM
#if defined(HAVE_GETPWUID) || defined(HAVE_GETPWNAM)
#include <pwd.h>
#endif
#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include "md5.h"

#include "FileInfo.h"
#include "Location.h"
#include "StateChange.h"
#include "binary.h"

#define HAVE_REALPATH
#ifdef HAVE_REALPATH
extern "C" char *realpath(const char *, char resolved_path[]);
#endif

namespace Attic {

Path Path::Combine(const Path& first, const Path& second)
{
  if (first.empty())
    return second;
  Path result(first);
  if (! second.empty())
    result += '/';
  result += second;
  return result;
}

Path Path::ExpandPath(const Path& path)
{
  char resolved_path[PATH_MAX];

  if (path.length() == 0 || path[0] != '~')
    return Path(realpath(path.c_str(), resolved_path));

  const char * pfx = NULL;
  std::string::size_type pos = path.find_first_of('/');

  if (path.length() == 1 || pos == 1) {
    pfx = std::getenv("HOME");
#ifdef HAVE_GETPWUID
    if (! pfx) {
      // Punt. We're trying to expand ~/, but HOME isn't set
      struct passwd * pw = getpwuid(getuid());
      if (pw)
	pfx = pw->pw_dir;
    }
#endif
  }
#ifdef HAVE_GETPWNAM
  else {
    std::string user(path, 1, pos == std::string::npos ?
		     std::string::npos : pos - 1);
    struct passwd * pw = getpwnam(user.c_str());
    if (pw)
      pfx = pw->pw_dir;
  }
#endif

  // if we failed to find an expansion, return the path unchanged.

  if (! pfx)
    return Path(realpath(path.c_str(), resolved_path));

  if (pos == std::string::npos)
    return Path(realpath(pfx, resolved_path));

  return Path(realpath(Combine(pfx, path.substr(pos + 1)).c_str(),
		       resolved_path));
}

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

void File::Copy(const Path& path, const Path& dest)
{
  assert(File::Exists(path));

  std::ifstream fin(path.c_str());
  std::ofstream fout(dest.c_str());

  do {
    char buf[8192];
    fin.read(buf, 8192);
    fout.write(buf, fin.gcount());
  } while (! fin.eof() && fin.good() && fout.good());

  fin.close();
  fout.close();

  assert(File::Exists(dest));
}

void FileInfo::dostat() const
{
  if (lstat(Pathname.c_str(), &info) == -1) {
    if (errno == ENOENT) {
      flags |= FILEINFO_DIDSTAT;
      return;
    }
    throw Exception("Failed to stat '" + Pathname + "'");
  }
  flags |= FILEINFO_DIDSTAT | FILEINFO_EXISTS;
}

FileInfo::~FileInfo()
{
  if (Children) {
    for (ChildrenMap::iterator i = Children->begin();
	 i != Children->end();
	 i++)
      delete (*i).second;
  }
}

void FileInfo::SetPath(const Path& _FullName)
{
  FullName = _FullName;
  Name     = Path::GetFileName(FullName);

  if (Repository)
    Pathname = Path::Combine(Repository->CurrentPath, FullName);
  else
    Pathname = FullName;

  Reset();
}

void FileInfo::GetFileInfos(ChildrenMap& store) const
{
  DIR * dirp = opendir(Pathname.c_str());
  if (dirp == NULL)
    return;

  struct dirent * dp;
  while ((dp = readdir(dirp)) != NULL) {
    if (dp->d_name[0] == '.' &&
	(dp->d_namlen == 1 ||
	 (dp->d_namlen == 2 && dp->d_name[1] == '.')))
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

void FileInfo::CopyDetails(FileInfo& dest, bool dataOnly)
{
  dest.flags	    = flags;
  dest.info.st_mode = info.st_mode;
  dest.Length()	    = Length();

  if (IsRegularFile() &&
      (dataOnly || flags & FILEINFO_READCSUM))
    dest.csum  = Checksum();
}

void FileInfo::CopyAttributes(FileInfo& dest, bool dataOnly)
{
  dest.flags |= FILEINFO_DIDSTAT;

  if (! dataOnly) {
    File::SetPermissions(dest.Pathname, Permissions());
    File::SetOwnership(dest.Pathname, OwnerId(), GroupId());
  }

  dest.SetPermissions(Permissions());

  dest.OwnerId() = OwnerId();
  dest.GroupId() = GroupId();

  if (! dataOnly)
    File::SetAccessTimes(dest.Pathname, LastAccessTime(), LastWriteTime());

  dest.LastAccessTime() = LastAccessTime();
  dest.LastWriteTime()  = LastWriteTime();
}

void FileInfo::CopyAttributes(const Path& dest)
{
  File::SetPermissions(dest, Permissions());
  File::SetOwnership(dest, OwnerId(), GroupId());
  File::SetAccessTimes(dest, LastAccessTime(), LastWriteTime());
}

md5sum_t& FileInfo::Checksum() const
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
  } else {
    File::Delete(FullName);
  }
  flags &= ~FILEINFO_EXISTS;
}

void FileInfo::Copy(const Path& dest)
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
  FileInfo * entry = NULL;

  unsigned char flags;
  read_binary_number(data, flags);

  if (! (flags & FILEINFO_VIRTUAL)) {
    Path name;
    read_binary_string(data, name);

    if (parent)
      name = Path::Combine(parent->FullName, name);

    entry = new FileInfo(name, parent, repository);
    entry->flags = flags;

    if (flags & FILEINFO_DIDSTAT && flags & FILEINFO_EXISTS)
      read_binary_number(data, entry->info);
    if (flags & FILEINFO_READCSUM)
      read_binary_number(data, entry->csum);
  } else {
    assert(! parent);
    entry = new FileInfo(repository);
    entry->flags = flags;
  }

  if (flags & FILEINFO_DIDSTAT && flags & FILEINFO_EXISTS &&
      (entry->info.st_mode & S_IFMT) == S_IFDIR) {
    int children = read_binary_long<int>(data);
    for (int i = 0; i < children; i++)
      ReadFrom(data, entry, repository);
  }
  return entry;
}

void FileInfo::DumpTo(std::ostream& out, int depth)
{
  for (int i = 0; i < depth; i++)
    out << "  ";

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

  out << FullName;

  if (IsRegularFile()) {
    out << " len " << Length();
    out << " csum " << Checksum();
  }
  if (Exists())
    out << " mod " << DateTime(LastWriteTime().tv_sec);

  out << std::endl;

  if (Children)
    for (ChildrenMap::iterator i = Children->begin();
	 i != Children->end();
	 i++)
      (*i).second->DumpTo(out, depth + 1);
}

void FileInfo::WriteTo(std::ostream& out)
{
  write_binary_number(out, flags);

  if (! IsVirtual()) {
    write_binary_string(out, Name);
    if (Exists())
      write_binary_number(out, info);
    if (IsRegularFile())
      write_binary_number(out, Checksum());
  }

  if (Exists() && IsDirectory()) {
    write_binary_long(out, (int)ChildrenSize());
    for (ChildrenMap::iterator i = ChildrenBegin();
	 i != ChildrenEnd();
	 i++)
      (*i).second->WriteTo(out);
  }
}

void FileInfo::PostAddChange(StateChangesMap& changesMap)
{
  if (IsDirectory()) {
    PostChange(changesMap, StateChange::Add, this, NULL);

    for (FileInfo::ChildrenMap::const_iterator i = ChildrenBegin();
	 i != ChildrenEnd();
	 i++)
      (*i).second->PostAddChange(changesMap);
  } else {
    PostChange(changesMap, StateChange::Add, this, NULL);
  }
}

void FileInfo::PostRemoveChange(const std::string& childName,
				FileInfo * ancestorChild,
				StateChangesMap& changesMap)
{
  FileInfo * missingChild =
    new FileInfo(Path::Combine(FullName, childName), this, Repository);

  if (ancestorChild->IsDirectory() && ancestorChild->Children)
    for (FileInfo::ChildrenMap::const_iterator
	   i = ancestorChild->ChildrenBegin();
	 i != ancestorChild->ChildrenEnd();
	 i++)
      missingChild->PostRemoveChange((*i).first, (*i).second, changesMap);

  PostChange(changesMap, StateChange::Remove, missingChild, ancestorChild);
}

void FileInfo::PostUpdateChange(FileInfo * ancestor,
				StateChangesMap& changesMap)
{
  PostChange(changesMap, StateChange::Update, this, ancestor);
}

void FileInfo::PostUpdatePropsChange(FileInfo * ancestor,
				     StateChangesMap& changesMap)
{
  PostChange(changesMap, StateChange::UpdateProps, this, ancestor);
}

void FileInfo::CompareTo(FileInfo * ancestor, StateChangesMap& changesMap)
{
  assert(Repository);

  if (! ancestor) {
    PostAddChange(changesMap);
    return;
  }

  if (! IsVirtual() && Name != ancestor->Name)
    throw Exception("Names do not match in comparison: " +
		    FullName + " != " + ancestor->Name);

  bool updateRegistered = false;

  if (FileKind() != ancestor->FileKind()) {
    PostUpdateChange(ancestor, changesMap);
    updateRegistered = true;
  }
  else if (IsRegularFile()) {
    if (Length() != ancestor->Length()) {
      PostUpdateChange(ancestor, changesMap);
      updateRegistered = true;
    }
    else if (! Repository->TrustLengthOnly) {
      if (LastWriteTime() != ancestor->LastWriteTime() ||
	  (! Repository->TrustTimestamps &&
	   Checksum() != ancestor->Checksum())) {
	PostUpdateChange(ancestor, changesMap);
	updateRegistered = true;
      }
    }
  }
  else if (Exists() && LastWriteTime() != ancestor->LastWriteTime()) {
    PostUpdatePropsChange(ancestor, changesMap);
    updateRegistered = true;
  }

  if (! updateRegistered && Exists() &&
      (Permissions() != ancestor->Permissions() ||
       OwnerId()     != ancestor->OwnerId()	||
       GroupId()     != ancestor->GroupId()))
    PostUpdatePropsChange(ancestor, changesMap);

  if (! IsDirectory())
    return;

  bool updateProps = false;

  for (ChildrenMap::iterator i = ChildrenBegin();
       i != ChildrenEnd();
       i++) {
    FileInfo * ancestorChild = ancestor->FindChild((*i).first);
    if (ancestorChild != NULL) {
      ancestorChild->flags |= FILEINFO_HANDLED;
      (*i).second->CompareTo(ancestorChild, changesMap);
    } else {
      (*i).second->PostAddChange(changesMap);
      updateProps = true;
    }
  }

  for (ChildrenMap::iterator i = ancestor->ChildrenBegin();
       i != ancestor->ChildrenEnd();
       i++) {
    if ((*i).second->flags & FILEINFO_HANDLED) {
      (*i).second->flags &= ~FILEINFO_HANDLED;
    }
    else if (FindChild((*i).first) == NULL) {
      PostRemoveChange((*i).first, (*i).second, changesMap);
      updateProps = true;
    }
  }

  if (updateProps && ! updateRegistered)
    PostUpdatePropsChange(ancestor, changesMap);
}

void Directory::CreateDirectory(const Path& path)
{
  const char * b = path.c_str();
  const char * p = b + 1;
  while (*p) {
    if (*p == '/')
      CreateDirectory(FileInfo(std::string(path, 0, p - b)));
    ++p;
  }
  CreateDirectory(FileInfo(path));
}

} // namespace Attic
