#ifndef _FILEINFO_H
#define _FILEINFO_H

#include <string>
#include <list>

#include <sys/stat.h>

#include "error.h"
#include "md5.h"

class Path
{
public:
  static int Parts(const std::string& path)
  {
    int len = path.length();
    int count = 1;
    for (int i = 0; i < len; i++)
      if (path[i] == '/')
	count++;
    return count;
  }

  static std::string GetFileName(const std::string& path)
  {
    int index = path.rfind('/');
    if (index != std::string::npos)
      return path.substr(index + 1);
    else      
      return path;
  }

  static std::string Combine(const std::string& first,
			     const std::string& second)
  {
    return first + "/" + second;
  }
};

class md5sum_t
{
  md5_byte_t digest[16];

public:
  bool operator==(const md5sum_t& other) const {
    return memcmp(digest, other.digest, sizeof(md5_byte_t) * 16) == 0;
  }
  bool operator!=(const md5sum_t& other) const {
    return ! (*this == other);
  }
  bool operator<(const md5sum_t& other) const {
    return memcmp(digest, other.digest, sizeof(md5_byte_t) * 16) < 0;
  }

  static md5sum_t checksum(const std::string& path, md5sum_t& csum);

  friend std::ostream& operator<<(std::ostream& out, const md5sum_t& md5);
};

class FileInfo
{
  mutable struct stat info;

public:
  mutable bool didstat;

  enum Kind {
    Collection, Directory, File, SymbolicLink, Special, Nonexistant, Last
  };

  std::string Name;
  std::string RelativeName;
  std::string FullName;

  FileInfo() : fileKind(Collection) {
    readcsum = false;
    didstat  = true;
  }

  FileInfo(const std::string& _FullName)
    : FullName(_FullName),
      RelativeName(_FullName),
      Name(Path::GetFileName(_FullName)) {
    Reset();
  }

  FileInfo(const std::string& _FullName, const std::string& _RelativeName)
    : FullName(_FullName),
      RelativeName(_RelativeName),
      Name(Path::GetFileName(_FullName)) {
    Reset();
  }

  mutable Kind fileKind;

  void dostat() const;

public:
  void Reset() {
    fileKind = Nonexistant;
    didstat  = false;
    readcsum = false;
  }

  Kind FileKind() const {
    if (! didstat) dostat();
    return fileKind;
  }

  bool Exists() const {
    return FileKind() != Nonexistant;
  }

  off_t& Length() {
    if (! didstat) dostat();
    assert(Exists());
    return info.st_size;
  }

  mode_t Permissions() {
    if (! didstat) dostat();
    return info.st_mode & ~S_IFMT;
  }    
  void SetPermissions(mode_t mode) {
    if (! didstat) dostat();
    info.st_mode = (info.st_mode & S_IFMT) | mode;
  }    
  uid_t& OwnerId() {
    if (! didstat) dostat();
    return info.st_uid;
  }    
  gid_t& GroupId() {
    if (! didstat) dostat();
    return info.st_gid;
  }    

  struct timespec& LastWriteTime() {
    if (! didstat) dostat();
    return info.st_mtimespec;
  }
  struct timespec& LastAccessTime() {
    if (! didstat) dostat();
    return info.st_atimespec;
  }

  bool IsDirectory() const {
    return FileKind() == Directory;
  }
  bool IsSymbolicLink() const {
    return FileKind() == SymbolicLink;
  }
  bool IsRegularFile() const {
    return FileKind() == File;
  }

  void CopyAttributes(FileInfo& dest, bool dataOnly = false);
  void CopyAttributes(const std::string& dest);

private:
  mutable md5sum_t csum;

public:
  mutable bool readcsum;

  md5sum_t& Checksum() {
    if (! IsRegularFile())
      throw Exception("Attempt to calc checksum of non-file '" + FullName + "'");

    if (! readcsum) {
      md5sum_t::checksum(FullName, csum);
      readcsum = true;
    }
    return csum;
  }

  md5sum_t CurrentChecksum() const {
    if (! IsRegularFile())
      throw Exception("Attempt to calc checksum of non-file '" + FullName + "'");

    md5sum_t sum;
    md5sum_t::checksum(FullName, sum);
    return sum;
  }

  FileInfo LinkReference() const {
    if (! IsSymbolicLink())
      throw Exception("Attempt to dereference non-symbol link '" + FullName + "'");

    char buf[8192];
    if (readlink(FullName.c_str(), buf, 8191) != -1)
      return FileInfo(buf);
    else
      throw Exception("Failed to read symbol link '" + FullName + "'");
  }

  void CreateDirectory();
  void GetFileInfos(std::list<FileInfo>& store);
  void Delete();
};

class File
{
public:
  static std::string ExpandPath(const std::string &path);

  static void Delete(const std::string& path) {
    if (unlink(path.c_str()) == -1)
      throw Exception("Failed to delete '" + path + "'");
  }

  static void Copy(const std::string& path, const std::string& dest);
  static void Move(const std::string& path, const std::string& dest)
  {
    if (rename(path.c_str(), dest.c_str()) == -1)
      throw Exception("Failed to move '" + path + "' to '" + dest + "'");
  }

  static void SetOwnership(const std::string& path,
			   mode_t mode, uid_t uid, gid_t gid)
  {
    if (chmod(path.c_str(), mode) == -1)
      throw Exception("Failed to change permissions of '" + path + "'");
    if (chown(path.c_str(), uid, gid) == -1)
      throw Exception("Failed to change ownership of '" + path + "'");
  }

  static void SetAccessTimes(const std::string& path,
			     struct timespec LastAccessTime,
			     struct timespec LastWriteTime)
  {
    struct timeval temp[2];
    TIMESPEC_TO_TIMEVAL(&temp[0], &LastAccessTime);
    TIMESPEC_TO_TIMEVAL(&temp[1], &LastWriteTime);

    if (utimes(path.c_str(), temp) == -1)
      throw Exception("Failed to set last write time of '" + path + "'");
  }
};

class Directory : public File
{
  static void CreateDirectory(const FileInfo& info)
  {
    if (! info.Exists())
      if (mkdir(info.FullName.c_str(), 0755) == -1)
	throw Exception("Failed to create directory '" + info.FullName + "'");
  }

public:
  static void CreateDirectory(const std::string& path)
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

  static void Delete(const std::string& path)
  {
    if (rmdir(path.c_str()) == -1)
      throw Exception("Failed to remove directory '" + path + "'");
  }

  static void Copy(const std::string& path, const std::string& dest)
  {
    assert(0);			// jww (2006-11-03): implement
  }

  static void Move(const std::string& path, const std::string& dest)
  {
    assert(0);			// jww (2006-11-03): implement
  }
};

inline bool operator==(struct timespec& a, struct timespec& b)
{
  return std::memcmp(&a, &b, sizeof(struct timespec)) == 0;
}

inline bool operator!=(struct timespec& a, struct timespec& b)
{
  return ! (a == b);
}

#endif // _FILEINFO_H
