#include <fstream>

#include <sys/types.h>
#include <sys/time.h>
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
  std::string result(first);
  if (result.length() == 0)
    return second;
  else if (! second.empty() && result[result.length() - 1] != '/')
    result += '/';
  result += second;
  return Path(result);
}

Path Path::ExpandPath(const Path& path)
{
  char resolved_path[PATH_MAX];

  if (path.Name.length() == 0 || path.Name[0] != '~')
    return Path(realpath(path.c_str(), resolved_path));

  const char * pfx = NULL;
  std::string::size_type pos = path.Name.find_first_of('/');

  if (path.Name.length() == 1 || pos == 1) {
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
    std::string user(path.Name, 1, pos == std::string::npos ?
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

  return Path(realpath(Combine(Path(pfx),
			       Path(path.Name.substr(pos + 1)).c_str()).c_str(),
		       resolved_path));
}

md5sum_t md5sum_t::checksum(const Path& path, md5sum_t& csum)
{
  md5_state_t state;
  md5_init(&state);

  char cbuf[8192];

  std::fstream fin(path.c_str());
  fin.read(cbuf, 8192);
  int read = fin.gcount();
  while (read > 0) {
    md5_append(&state, (md5_byte_t *)cbuf, read);
    if (fin.eof() || fin.bad())
      break;
    fin.read(cbuf, 8192);
    read = fin.gcount();
  }

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
  std::ifstream fin(path.c_str());
  std::ofstream fout(dest.c_str());
  do {
    char buf[8192];
    fin.read(buf, 8192);
    fout.write(buf, fin.gcount());
  } while (! fin.eof() && fin.good() && fout.good());
}

FileInfo::FileInfo(Location * _Repository)
  : Repository(_Repository), fileKind(Collection),
    Parent(NULL), Children(NULL)
{
  flags = FILEINFO_DIDSTAT;
}

FileInfo::FileInfo(const Path& _FullName, FileInfo * _Parent,
		   Location * _Repository)
  : Repository(_Repository), Parent(_Parent), Children(NULL)
{
  SetPath(_FullName);

  if (Parent)
    Parent->AddChild(this);
}

#if 0
FileInfo::FileInfo(const FileInfo& other) : Children(NULL)
{
  flags = other.flags;
  if (flags & FILEINFO_DIDSTAT)
    std::memcpy(&info, &other.info, sizeof(struct stat));
  if (flags & FILEINFO_READCSUM)
    csum = other.csum;

  fileKind   = other.fileKind;
  Name	     = other.Name;
  FullName   = other.FullName;
  Repository = other.Repository;
  Pathname   = other.Pathname;
}
#endif

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

void FileInfo::dostat() const
{
  if (lstat(Pathname.c_str(), &info) == -1 && errno == ENOENT) {
    fileKind = Nonexistant;
    return;
  }

  flags |= FILEINFO_DIDSTAT;

  switch (info.st_mode & S_IFMT) {
  case S_IFDIR:
    fileKind = Directory;
    break;

  case S_IFREG:
    fileKind = File;
    break;

  case S_IFLNK:
    fileKind = SymbolicLink;
    break;

  default:
    fileKind = Special;
    break;
  }
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
  if (FileKind() != Directory && FileKind() != Collection)
    throw Exception("Attempt to call ChildrenSize on a non-collection");

  if (! Children) {
    Children = new ChildrenMap;
    GetFileInfos(*Children);
  }
  return Children->size();
}

FileInfo::ChildrenMap::iterator FileInfo::ChildrenBegin() const
{
  if (FileKind() != Directory && FileKind() != Collection)
    throw Exception("Attempt to call ChildrenBegin on a non-collection");

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
  for (std::string::size_type index = path.Name.find('/', previ);
       index != std::string::npos;
       previ = index + 1, index = path.Name.find('/', previ)) {
    std::string part(path.Name, previ, index - previ);
    current = current->FindChild(part);
    if (! current)
      return NULL;
  }

  return current->FindChild(std::string(path.Name, previ));
}

FileInfo * FileInfo::FindOrCreateMember(const Path& path)
{
  if (path.empty())
    return this;

  FileInfo * current = this;

  std::string::size_type previ = 0;
  for (std::string::size_type index = path.Name.find('/', previ);
       index != std::string::npos;
       previ = index + 1, index = path.Name.find('/', previ)) {
    std::string part(path.Name, previ, index - previ);
    current = current->FindOrCreateChild(part);
  }

  return current->FindOrCreateChild(std::string(path.Name, previ));
}

void FileInfo::CopyDetails(FileInfo& dest, bool dataOnly)
{
  dest.flags |= FILEINFO_DIDSTAT;

  dest.fileKind	    = FileKind();
  dest.info.st_mode = info.st_mode;

  dest.Length() = Length();

  if (IsRegularFile() &&
      (dataOnly || flags & FILEINFO_READCSUM))
    csum = Checksum();
}

void FileInfo::CopyAttributes(FileInfo& dest, bool dataOnly)
{
  dest.flags |= FILEINFO_DIDSTAT;

  if (! dataOnly)
    File::SetOwnership(dest.Pathname, Permissions(), OwnerId(), GroupId());

  dest.OwnerId() = OwnerId();
  dest.GroupId() = GroupId();

  if (! dataOnly)
    File::SetAccessTimes(dest.Pathname, LastAccessTime(), LastWriteTime());

  dest.LastAccessTime() = LastAccessTime();
  dest.LastWriteTime()  = LastWriteTime();
}

void FileInfo::CopyAttributes(const Path& dest)
{
  File::SetOwnership(dest, Permissions(), OwnerId(), GroupId());
  File::SetAccessTimes(dest, LastAccessTime(), LastWriteTime());
}

md5sum_t& FileInfo::Checksum()
{
  if (! IsRegularFile())
    throw Exception("Attempt to calc checksum of non-file '" + Pathname + "'");

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
  int index = Pathname.Name.rfind('/');
  if (index != std::string::npos)
    Directory::CreateDirectory(std::string(Pathname.Name, 0, index));
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
}

FileInfo * FileInfo::ReadFrom(char *& data, FileInfo * parent,
			      Location * repository)
{
  FileInfo * info = NULL;

  int kind;
  read_binary_long(data, kind);
  Kind fileKind = static_cast<FileInfo::Kind>(kind);
  assert(fileKind < FileInfo::Last);

  if (kind != FileInfo::Collection) {
    Path name;
    read_binary_string(data, name.Name);

    if (parent)
      name = parent->FullName + name;

    info = new FileInfo(name, parent, repository);
    info->fileKind = fileKind;

    if (fileKind != FileInfo::Nonexistant) {
      info->flags |= FILEINFO_DIDSTAT;

      read_binary_number(data, info->LastAccessTime());
      read_binary_number(data, info->LastWriteTime());

      mode_t mode;
      read_binary_number(data, mode);
      info->SetPermissions(mode);

      read_binary_number(data, info->OwnerId());
      read_binary_number(data, info->GroupId());

      if (kind == FileInfo::File) {
	read_binary_long(data, info->Length());

	info->flags |= FILEINFO_READCSUM;
	read_binary_number(data, info->Checksum());
      }
    }
  } else {
    assert(! parent);
    info = new FileInfo(repository);
    assert(info->fileKind == fileKind);
  }

  if (fileKind == FileInfo::Collection ||
      fileKind == FileInfo::Directory) {
    int children = read_binary_long<int>(data);
    for (int i = 0; i < children; i++)
      ReadFrom(data, info, repository);
  }
  return info;
}

void FileInfo::DumpTo(std::ostream& out, int depth)
{
  for (int i = 0; i < depth; i++)
    out << "  ";

  switch (FileKind()) {
  case Collection:   out << "COL: "; break;
  case Directory:    out << "DIR: "; break;
  case File:         out << "FIL: "; break;
  case SymbolicLink: out << "SYM: "; break;
  case Special:      out << "SPC: "; break;
  case Nonexistant:  out << "NON: "; break;
  default:
    assert(0);
    break;
  }

  out << FullName;

  if (IsRegularFile())
    out << " len " << Length();

  if (FileKind() != Collection &&
      FileKind() != Nonexistant)
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
  FileInfo::Kind fileKind = FileKind();

  write_binary_long(out, static_cast<int>(fileKind));

  if (fileKind != FileInfo::Collection) {
    write_binary_string(out, Name);

    if (fileKind != FileInfo::Nonexistant) {
      write_binary_number(out, LastAccessTime());
      write_binary_number(out, LastWriteTime());
      write_binary_number(out, Permissions());
      write_binary_number(out, OwnerId());
      write_binary_number(out, GroupId());

      if (fileKind == FileInfo::File) {
	write_binary_long(out, Length());
	write_binary_number(out, Checksum());
      }
    }
  }

  if (fileKind == FileInfo::Collection ||
      fileKind == FileInfo::Directory) {
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

void FileInfo::PostRemoveChange(FileInfo * ancestor,
				StateChangesMap& changesMap)
{
  if (IsDirectory())
    for (FileInfo::ChildrenMap::const_iterator i = ChildrenBegin();
	 i != ChildrenEnd();
	 i++) {
      FileInfo * ancestorChild = ancestor->FindChild((*i).first);
      if (ancestorChild)
	(*i).second->PostRemoveChange(ancestorChild, changesMap);
    }

  PostChange(changesMap, StateChange::Remove, this, ancestor);
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

  FileInfo::Kind fileKind = FileKind();

  if (fileKind != FileInfo::Collection && Name != ancestor->Name)
    throw Exception("Names do not match in comparison: " +
		    FullName + " != " + ancestor->Name);

  bool updateRegistered = false;

  if (fileKind != ancestor->FileKind()) {
    PostUpdateChange(ancestor, changesMap);
    updateRegistered = true;
  }
  else if (fileKind == FileInfo::File) {
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
  else if (fileKind != FileInfo::Collection &&
	   fileKind != FileInfo::Nonexistant &&
	   LastWriteTime() != ancestor->LastWriteTime()) {
    PostUpdatePropsChange(ancestor, changesMap);
    updateRegistered = true;
  }

  if (! updateRegistered &&
      fileKind != FileInfo::Collection &&
      fileKind != FileInfo::Nonexistant &&
      (Permissions() != ancestor->Permissions() ||
       OwnerId()     != ancestor->OwnerId()	||
       GroupId()     != ancestor->GroupId()))
    PostUpdatePropsChange(ancestor, changesMap);

  if (fileKind != FileInfo::Collection &&
      fileKind != FileInfo::Directory)
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
      FileInfo * missingChild = new FileInfo(FullName + (*i).first, this,
					     Repository);
      missingChild->PostRemoveChange((*i).second, changesMap);
      updateProps = true;
    }
  }

#if 0
  if (updateProps && ! updateRegistered)
    PostUpdatePropsChange(ancestor, changesMap);
#endif
}

} // namespace Attic
