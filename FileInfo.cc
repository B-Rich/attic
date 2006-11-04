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

md5sum_t md5sum_t::checksum(const std::string& path, md5sum_t& csum)
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

void File::Copy(const std::string& path, const std::string& dest)
{
  std::ifstream fin(path.c_str());
  std::ofstream fout(dest.c_str());
  do {
    char buf[8192];
    fin.read(buf, 8192);
    fout.write(buf, fin.gcount());
  } while (! fin.eof() && fin.good() && fout.good());
}

std::string File::ExpandPath(const std::string& path)
{
  if (path.length() == 0 || path[0] != '~') {
    char resolved_path[PATH_MAX];
    return realpath(path.c_str(), resolved_path);
  }

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

  if (! pfx) {
    char resolved_path[PATH_MAX];
    return realpath(path.c_str(), resolved_path);
  }

  if (pos == std::string::npos) {
    char resolved_path[PATH_MAX];
    return realpath(pfx, resolved_path);
  }

  std::string result(pfx);
  if (result.length() == 0 || result[result.length() - 1] != '/')
    result += '/';
  result += path.substr(pos + 1);

  char resolved_path[PATH_MAX];
  return realpath(result.c_str(), resolved_path);
}

void FileInfo::dostat() const
{
  if (lstat(FullName.c_str(), &info) == -1 && errno == ENOENT) {
    fileKind = Nonexistant;
    return;
  }

  didstat = true;

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

void FileInfo::GetFileInfos(std::list<FileInfo>& store)
{
  DIR * dirp = opendir(FullName.c_str());
  if (dirp == NULL)
    return;
  struct dirent * dp;
  while ((dp = readdir(dirp)) != NULL) {
    if (dp->d_name[0] == '.' &&
	(dp->d_namlen == 1 ||
	 (dp->d_namlen == 2 && dp->d_name[1] == '.')))
      continue;

    store.push_back(FileInfo());
    store.back().Name = dp->d_name;
    store.back().FullName = Path::Combine(FullName, dp->d_name);
    store.back().RelativeName = Path::Combine(RelativeName, dp->d_name);
  }
  (void)closedir(dirp);
}

void FileInfo::CopyAttributes(FileInfo& dest, bool dataOnly)
{
  if (! dataOnly)
    File::SetOwnership(dest.FullName, Permissions(), OwnerId(), GroupId());

  dest.SetPermissions(Permissions());

  dest.OwnerId() = OwnerId();
  dest.GroupId() = GroupId();

  if (! dataOnly)
    File::SetAccessTimes(dest.FullName, LastAccessTime(), LastWriteTime());

  dest.LastAccessTime() = LastAccessTime();
  dest.LastWriteTime()  = LastWriteTime();
}

void FileInfo::CopyAttributes(const std::string& dest)
{
  File::SetOwnership(dest, Permissions(), OwnerId(), GroupId());
  File::SetAccessTimes(dest, LastAccessTime(), LastWriteTime());
}

void FileInfo::CreateDirectory()
{
  int index = FullName.rfind('/');
  if (index != std::string::npos)
    Directory::CreateDirectory(std::string(FullName, 0, index));
}

void FileInfo::Delete()
{
  if (IsDirectory()) {
    std::list<FileInfo> infos;
    GetFileInfos(infos);
    for (std::list<FileInfo>::iterator i = infos.begin();
	 i != infos.end();
	 i++)
      (*i).Delete();
    Directory::Delete(FullName);
  } else {
    File::Delete(FullName);
  }
}
