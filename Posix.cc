#include "Posix.h"
#include "Location.h"

#include <fstream>
#include <cstdlib>
#include <cstring>

#include <dirent.h>
#include <errno.h>

#if defined(HAVE_GETPWUID) || defined(HAVE_GETPWNAM)
#include <pwd.h>
#endif

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace Attic {

FileInfo::Kind PosixFileInfo::FileKind() const
{
  if (! HasFlags(FILEINFO_READATTR))
    Repository->SiteBroker->ReadAttributes(const_cast<PosixFileInfo&>(*this));

  switch (info.st_mode & S_IFMT) {
  case S_IFIFO:			/* [XSI] named pipe (fifo) */
    return NamedPipe;
  case S_IFCHR:			/* [XSI] character special */
    return CharDevice;
  case S_IFDIR:			/* [XSI] directory */
    return Directory;
  case S_IFBLK:			/* [XSI] block special */
    return BlockDevice;
  case S_IFREG:			/* [XSI] regular */
    return RegularFile;
  case S_IFLNK:			/* [XSI] symbolic link */
    return SymbolicLink;
  case S_IFSOCK:		/* [XSI] socket */
    return Socket;
  default:
    return Special;
  }
}

bool PosixFileInfo::IsReadable() const
{
  return static_cast<PosixVolumeBroker *>
    (Repository->SiteBroker)->IsReadable(Pathname);
}

bool PosixFileInfo::IsWritable() const
{
  return static_cast<PosixVolumeBroker *>
    (Repository->SiteBroker)->IsWritable(Pathname);
}
  
bool PosixFileInfo::IsSearchable() const
{
  return static_cast<PosixVolumeBroker *>
    (Repository->SiteBroker)->IsSearchable(Pathname);
}

unsigned long long PosixFileInfo::Length() const
{
  const_cast<PosixFileInfo&>(*this).ReadAttributes();

  if (! Exists())
    throw Exception("Attempt to determine length of non-existant file '" +
		    Moniker() + "'");
  else if (! IsRegularFile())
    throw Exception("Attempt to determine length of non-regular file '" +
		    Moniker() + "'");

  return info.st_size;
}

mode_t PosixFileInfo::Permissions() const
{
  const_cast<PosixFileInfo&>(*this).ReadAttributes();
  if (! Exists())
    throw Exception("Attempt to read permissions of non-existant item '" +
		    Moniker() + "'");
  return info.st_mode & ~S_IFMT;
}    

void PosixFileInfo::SetPermissions(const mode_t& mode)
{
  info.st_mode = (((info.st_mode & S_IFMT) | mode) | info.st_mode & ~S_IFMT);
  posixFlags |= POSIX_FILEINFO_MODECHG;
}    

const uid_t& PosixFileInfo::OwnerId() const
{
  const_cast<PosixFileInfo&>(*this).ReadAttributes();
  if (! Exists())
    throw Exception("Attempt to read owner id of non-existant item '" +
		    Moniker() + "'");
  return info.st_uid;
}    

void PosixFileInfo::SetOwnerId(const uid_t& uid) const
{
  info.st_uid = uid;
  posixFlags |= POSIX_FILEINFO_OWNRCHG;
}    

const gid_t& PosixFileInfo::GroupId() const
{
  const_cast<PosixFileInfo&>(*this).ReadAttributes();
  if (! Exists())
    throw Exception("Attempt to read group id of non-existant item '" +
		    Moniker() + "'");
  return info.st_gid;
}    

void PosixFileInfo::SetGroupId(const gid_t& gid)
{
  info.st_gid = gid;
  posixFlags |= POSIX_FILEINFO_OWNRCHG;
}    

DateTime PosixFileInfo::LastWriteTime() const
{
  const_cast<PosixFileInfo&>(*this).ReadAttributes();
  if (! Exists())
    throw Exception("Attempt to read last write time of non-existant item '" +
		    Moniker() + "'");
#ifdef STAT_USES_ST_ATIM
  return DateTime(info.st_mtim);
#else
#ifdef STAT_USES_ST_ATIMESPEC
  return DateTime(info.st_mtimespec);
#else
#ifdef STAT_USES_ST_ATIMENSEC
  return DateTime(info.st_mtime, info.st_mtimensec);
#else
#ifdef STAT_USES_ST_ATIME
  return DateTime(info.st_mtime);
#endif
#endif
#endif
#endif
}

void PosixFileInfo::SetLastWriteTime(const DateTime& when)
{
#ifdef STAT_USES_ST_ATIM
  info.st_mtim = when;
#else
#ifdef STAT_USES_ST_ATIMESPEC
  info.st_mtimespec = when;
#else
#ifdef STAT_USES_ST_ATIMENSEC
  info.st_mtime     = when.secs;
  info.st_mtimensec = when.nsecs;
#else
#ifdef STAT_USES_ST_ATIME
  info.st_mtime = when;
#endif
#endif
#endif
#endif
  posixFlags |= POSIX_FILEINFO_TIMECHG;
}

DateTime PosixFileInfo::LastAccessTime() const
{
  const_cast<PosixFileInfo&>(*this).ReadAttributes();
  if (! Exists())
    throw Exception("Attempt to read last access time of non-existant item '" +
		    Moniker() + "'");
#ifdef STAT_USES_ST_ATIM
  return DateTime(info.st_atim);
#else
#ifdef STAT_USES_ST_ATIMESPEC
  return DateTime(info.st_atimespec);
#else
#ifdef STAT_USES_ST_ATIMENSEC
  return DateTime(info.st_atime, info.st_atimensec);
#else
#ifdef STAT_USES_ST_ATIME
  return DateTime(info.st_atime);
#endif
#endif
#endif
#endif
}

void PosixFileInfo::SetLastAccessTime(const DateTime& when)
{
#ifdef STAT_USES_ST_ATIM
  info.st_atim = when;
#else
#ifdef STAT_USES_ST_ATIMESPEC
  info.st_atimespec = when;
#else
#ifdef STAT_USES_ST_ATIMENSEC
  info.st_atime     = when.secs;
  info.st_atimensec = when.nsecs;
#else
#ifdef STAT_USES_ST_ATIME
  info.st_atime = when;
#endif
#endif
#endif
#endif
  posixFlags |= POSIX_FILEINFO_TIMECHG;
}

Path PosixFileInfo::LinkTarget() const
{
  const_cast<PosixFileInfo&>(*this).ReadAttributes();

  if (! Exists())
    throw Exception("Attempt to read link reference of non-existant item '" +
		    Moniker() + "'");
  else if (! IsSymbolicLink())
    throw Exception("Attempt to dereference non-symbol link '" + Moniker() + "'");

  assert(LinkTargetPath);
  return *LinkTargetPath;
}

void PosixFileInfo::SetLinkTarget(const Path& path)
{
  if (! IsSymbolicLink())
    throw Exception("Attempt to set link target of non-symbolic link '" +
		    Moniker() + "'");

  if (LinkTargetPath)
    delete LinkTargetPath;

  LinkTargetPath = new Path(path);
  posixFlags |= POSIX_FILEINFO_LINKCHG;
}

bool PosixFileInfo::CompareAttributes(const FileInfo& other) const
{
  assert(FileKind() == other.FileKind());

  const PosixFileInfo * otherInfo =
    dynamic_cast<const PosixFileInfo *>(&other);
  if (! otherInfo)
    return false;

  return (Permissions() == otherInfo->Permissions() &&
	  OwnerId()	== otherInfo->OwnerId()	 &&
	  GroupId()	== otherInfo->GroupId()	 &&
	  (! IsSymbolicLink() ||
	   LinkTarget() == otherInfo->LinkTarget()));
}

void PosixFileInfo::CopyAttributes(FileInfo& dest) const
{
  PosixFileInfo * destInfo = dynamic_cast<PosixFileInfo *>(&dest);
  if (! destInfo)
    throw Exception("Attempt to copy POSIX attributes to non-POSIX file");

  if (destInfo->Permissions() != Permissions())
    destInfo->SetPermissions(Permissions());

  if (destInfo->OwnerId() != OwnerId())
    destInfo->SetOwnerId(OwnerId());
  if (destInfo->GroupId() != GroupId())
    destInfo->SetGroupId(GroupId());

  if (destInfo->LastAccessTime() != LastAccessTime())
    destInfo->SetLastAccessTime(LastAccessTime());
  if (destInfo->LastWriteTime() != LastWriteTime())
    destInfo->SetLastWriteTime(LastWriteTime());

  FileInfo::CopyAttributes(dest);
}

void PosixFileInfo::Dump(std::ostream& out, bool verbose, int depth) const
{
  for (int i = 0; i < depth; i++)
    out << "  ";

  if (verbose) {
    switch (FileKind()) {
    case Nonexistant:
      out << "Nonexistant: "; break;
    case RegularFile:
      out << "RegularFile: "; break;
    case Directory:
      out << "Directory: "; break;
    case SymbolicLink:
      out << "SymbolicLink: "; break;
    case CharDevice:
      out << "CharDevice: "; break;
    case BlockDevice:
      out << "BlockDevice: "; break;
    case NamedPipe:
      out << "NamedPipe: "; break;
    case Socket:
      out << "Socket: "; break;
    case Special:
      out << "Special: "; break;
    default:
      out << "Unknown: "; break;
    }
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

  for (ChildrenMap::iterator i = ChildrenBegin();
       i != ChildrenEnd();
       i++)
    (*i).second->Dump(out, verbose, depth + 1);
}

void PosixVolumeBroker::SetPermissions(const Path& path, mode_t mode)
{
  if (chmod(path.c_str(), mode) == -1)
    throw Exception("Failed to change permissions of '" + path + "'");
}

void PosixVolumeBroker::SetOwnership(const Path& path, uid_t uid, gid_t gid)
{
  if (chown(path.c_str(), uid, gid) == -1)
    throw Exception("Failed to change ownership of '" + path + "'");
}

void PosixVolumeBroker::SetAccessTimes(const Path& path,
				       const DateTime& LastAccessTime,
				       const DateTime& LastWriteTime)
{
  struct timeval temp[2];

  temp[0] = LastAccessTime;
  temp[1] = LastWriteTime;

  if (utimes(path.c_str(), temp) == -1)
    throw Exception("Failed to set access times of '" + path + "'");
}

void PosixVolumeBroker::SetLinkTarget(const Path& path, const Path& dest)
{
  if (Exists(path))
    DeleteFile(path);

  if (symlink(dest.c_str(), path.c_str()) == -1)
    throw Exception("Failed to create symbol link '" + path + "' to '" + dest + "'");
}

void PosixVolumeBroker::CreateFile(const PosixFileInfo& entry)
{
}

void PosixVolumeBroker::DeleteFile(const Path& path)
{
  if (unlink(path.c_str()) == -1)
    throw Exception("Failed to delete file '" + path + "'");
}

void PosixVolumeBroker::CopyFile(const PosixFileInfo& entry, const Path& dest)
{
  assert(entry.Exists());

  std::ifstream fin(entry.Pathname.c_str());
  std::ofstream fout(dest.c_str());

  do {
    char buf[8192];
    fin.read(buf, 8192);
    fout.write(buf, fin.gcount());
  }
  while (! fin.eof() && fin.good() && fout.good());

  fin.close();
  fout.close();

  assert(Exists(dest));
}

void PosixVolumeBroker::UpdateFile(const PosixFileInfo& entry,
				   const PosixFileInfo& dest)
{
}

void PosixVolumeBroker::MoveFile(const PosixFileInfo& entry, const Path& dest)
{
  if (rename(entry.Pathname.c_str(), dest.c_str()) == -1)
    throw Exception("Failed to move '" + entry.Moniker() + "' to '" + dest + "'");
}

void PosixVolumeBroker::DeleteDirectory(const Path& path)
{
  if (rmdir(path.c_str()) == -1)
    throw Exception("Failed to remove directory '" + path + "'");
}

void PosixVolumeBroker::CopyDirectory(const PosixFileInfo& entry,
				      const Path& dest)
{
}

void PosixVolumeBroker::MoveDirectory(const PosixFileInfo& entry,
				      const Path& dest)
{
}

bool PosixVolumeBroker::Exists(const Path& path) const
{
  return access(path.c_str(), F_OK) != -1 || errno != ENOENT;
}

bool PosixVolumeBroker::IsReadable(const Path& path) const
{
  return access(path.c_str(), R_OK) != -1;
}

bool PosixVolumeBroker::IsWritable(const Path& path) const
{
  return access(path.c_str(), W_OK) != -1;
}

bool PosixVolumeBroker::IsSearchable(const Path& path) const
{
  return access(path.c_str(), X_OK) != -1;
}

void PosixVolumeBroker::ReadAttributes(FileInfo& entry) const
{
  PosixFileInfo& posixEntry = static_cast<PosixFileInfo&>(entry);

  if (lstat(entry.Pathname.c_str(), &posixEntry.info) == -1) {
    if (errno == ENOENT) {
      posixEntry.SetFlags(FILEINFO_READATTR);
      return;
    }
    throw Exception("Failed to stat '" + posixEntry.Pathname + "'");
  }

  posixEntry.SetFlags(FILEINFO_READATTR | FILEINFO_EXISTS);

  if (posixEntry.IsSymbolicLink()) {
    char buf[8192];
    if (readlink(posixEntry.Pathname.c_str(), buf, 8191) != -1)
      posixEntry.LinkTargetPath = new Path(buf);
    else
      throw Exception("Failed to read symbol link '" + posixEntry.Pathname + "'");
  }
}

void PosixVolumeBroker::SyncAttributes(const FileInfo& entry)
{
  const PosixFileInfo& posixEntry = static_cast<const PosixFileInfo&>(entry);

  if (posixEntry.posixFlags & POSIX_FILEINFO_LINKCHG)
    SetLinkTarget(posixEntry.Pathname, posixEntry.LinkTarget());

  if (posixEntry.posixFlags & POSIX_FILEINFO_MODECHG)
    SetPermissions(posixEntry.Pathname, posixEntry.Permissions());

  if (posixEntry.posixFlags & POSIX_FILEINFO_OWNRCHG)
    SetOwnership(posixEntry.Pathname, posixEntry.OwnerId(),
		 posixEntry.GroupId());

  if (posixEntry.posixFlags & POSIX_FILEINFO_TIMECHG)
    SetAccessTimes(posixEntry.Pathname, posixEntry.LastAccessTime(),
		   posixEntry.LastWriteTime());
}

void PosixVolumeBroker::CopyAttributes(const FileInfo& entry, const Path& dest)
{
  const PosixFileInfo& posixEntry = static_cast<const PosixFileInfo&>(entry);

  if (posixEntry.IsSymbolicLink())
    SetLinkTarget(dest, posixEntry.LinkTarget());
  SetPermissions(dest, posixEntry.Permissions());
  SetOwnership(dest, posixEntry.OwnerId(), posixEntry.GroupId());
  SetAccessTimes(dest, posixEntry.LastAccessTime(), posixEntry.LastWriteTime());
}

void PosixVolumeBroker::ComputeChecksum(const Path& path, md5sum_t& csum) const
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

void PosixVolumeBroker::ReadDirectory(FileInfo& entry) const
{
  PosixFileInfo& posixEntry = static_cast<PosixFileInfo&>(entry);

  assert(posixEntry.Exists() &&
	 posixEntry.IsDirectory() &&
	 posixEntry.IsReadable());

  DIR * dirp = opendir(posixEntry.Pathname.c_str());
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
    CreateFileInfo(Path::Combine(posixEntry.FullName, dp->d_name),
		   &posixEntry);
  }

  (void)closedir(dirp);
}

void PosixVolumeBroker::CreateDirectory(const Path& path)
{
  const char * b = path.c_str();
  const char * p = b + 1;
  while (*p) {
    if (*p == '/') {
      Path subentry(std::string(b, 0, p - b));
      if (! PosixVolumeBroker::Exists(subentry))
	if (mkdir(subentry.c_str(), 0755) == -1)
	  throw Exception("Failed to create directory '" + subentry + "'");
    }
    ++p;
  }
  if (! PosixVolumeBroker::Exists(path))
    if (mkdir(path.c_str(), 0755) == -1)
      throw Exception("Failed to create directory '" + path + "'");
}

void PosixVolumeBroker::Create(const FileInfo& entry)
{
}

void PosixVolumeBroker::Delete(const FileInfo& entry)
{
#if 0
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
#endif
}

void PosixVolumeBroker::Copy(const FileInfo& entry, const Path& dest)
{
#if 0
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
#endif
}

void PosixVolumeBroker::Move(FileInfo& entry, const Path& dest)
{
#if 0
  File::Move(Pathname, dest);
#endif
}

} // namespace Attic
