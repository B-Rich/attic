#ifndef _PATH_H
#define _PATH_H

#include <string>

namespace Attic {

class Path : public std::string
{
public:
  Path() {}
  Path(const char * name) : std::string(name) {}
  Path(const std::string& name) : std::string(name) {}

  static Path ExpandPath(const Path& path);
  static Path Combine(const Path& first, const Path& second) {
    Path temp(first);
    temp += second;
    return temp;
  }

  Path& operator+=(const Path& other) {
    if (! empty() && (*this)[length() - 1] != '/' &&
	! other.empty() && other[0] != '/')
      this->std::string::operator +=("/");
    this->std::string::operator +=(other);
    return *this;
  }

  static std::string GetFileName(const Path& path)
  {
    int index = path.rfind('/');
    if (index != std::string::npos)
      return path.substr(index + 1);
    else      
      return path;
  }

  std::string FileName() const {
    return GetFileName(*this);
  }

  static Path GetDirectoryName(const Path& path)
  {
    int index = path.rfind('/');
    if (index != std::string::npos)
      return path.substr(0, index);
    else      
      return "";
  }

  Path DirectoryName() const {
    return GetDirectoryName(*this);
  }

#if 0
  static int PartsCount(const Path& path)
  {
    int len = path.length();
    int count = 1;
    for (int i = 0; i < len; i++)
      if (path[i] == '/')
	count++;
    return count;
  }
#endif
};

} // namespace Attic

#endif // _PATH_H
