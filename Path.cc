#include "Path.h"
#include "FileInfo.h"

#ifdef HAVE_REALPATH
extern "C" char *realpath(const char *, char resolved_path[]);
#endif

namespace Attic {

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

Path& Path::operator+=(const Path& other)
{
  bool endsWithSlash	    = ! empty() && (*this)[length() - 1] == '/';
  bool otherBeginsWithSlash = ! other.empty() && other[0] == '/';

  if (! endsWithSlash && ! otherBeginsWithSlash)
    this->std::string::operator +=("/");

  if (endsWithSlash && otherBeginsWithSlash)
    this->std::string::operator +=(other.substr(1));
  else
    this->std::string::operator +=(other);

  return *this;
}

} // namespace Attic
