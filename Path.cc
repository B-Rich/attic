#include "Path.h"
#include "FileInfo.h"

#include <fstream>
#include <cstdlib>

#if defined(HAVE_GETPWUID) || defined(HAVE_GETPWNAM)
#include <pwd.h>
#endif
#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

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

bool File::Exists(const Path& path)
{
  return FileInfo(path).Exists();
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

void File::SetAccessTimes(const Path& path,
			  const DateTime& LastAccessTime,
			  const DateTime& LastWriteTime)
{
  struct timeval temp[2];

  temp[0] = LastAccessTime;
  temp[1] = LastWriteTime;

  if (utimes(path.c_str(), temp) == -1)
    throw Exception("Failed to set last write time of '" + path + "'");
}

void Directory::CreateDirectory(const Path& path)
{
  const char * b = path.c_str();
  const char * p = b + 1;
  while (*p) {
    if (*p == '/') {
      Path subpath(std::string(path, 0, p - b));
      if (! File::Exists(subpath))
	if (mkdir(subpath.c_str(), 0755) == -1)
	  throw Exception("Failed to create directory '" + subpath + "'");
    }
    ++p;
  }
  if (! File::Exists(path))
    if (mkdir(path.c_str(), 0755) == -1)
      throw Exception("Failed to create directory '" + path + "'");
}

} // namespace Attic
