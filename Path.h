#ifndef _PATH_H
#define _PATH_H

#include "DateTime.h"
#include "error.h"

#include <string>

#include <sys/stat.h>

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

class File
{
public:
  static bool Exists(const Path& path);
  static void Delete(const Path& path) {
    if (unlink(path.c_str()) == -1)
      throw Exception("Failed to delete '" + path + "'");
  }

  static void Copy(const Path& path, const Path& dest);
  static void Move(const Path& path, const Path& dest) {
    if (rename(path.c_str(), dest.c_str()) == -1)
      throw Exception("Failed to move '" + path + "' to '" + dest + "'");
  }

  static void SetPermissions(const Path& path, mode_t mode) {
    if (chmod(path.c_str(), mode) == -1)
      throw Exception("Failed to change permissions of '" + path + "'");
  }

  static void SetOwnership(const Path& path, uid_t uid, gid_t gid) {
    if (chown(path.c_str(), uid, gid) == -1)
      throw Exception("Failed to change ownership of '" + path + "'");
  }

  static void SetAccessTimes(const Path& path,
			     const DateTime& LastAccessTime,
			     const DateTime& LastWriteTime);
};

class Directory : public File
{
public:
  static void CreateDirectory(const Path& path);
  static void Delete(const Path& path) {
    if (rmdir(path.c_str()) == -1)
      throw Exception("Failed to remove directory '" + path + "'");
  }
};

} // namespace Attic

#endif // _PATH_H
