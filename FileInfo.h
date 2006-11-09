#ifndef _FILEINFO_H
#define _FILEINFO_H

#include "DateTime.h"
#include "StateChange.h"

#include <string>
#include <map>

#include <sys/stat.h>

#include "error.h"
#include "md5.h"

namespace Attic {

class Path
{
public:
  std::string Name;

  Path() {}
  Path(const char * _Name) : Name(_Name) {}
  Path(const std::string& _Name) : Name(_Name) {}
  Path(const Path& other) : Name(other.Name) {}

  Path& operator=(const Path& other) {
    Name = other.Name;
  }
  
  static Path ExpandPath(const Path& path);

  static int Parts(const Path& path)
  {
    int len = path.Name.length();
    int count = 1;
    for (int i = 0; i < len; i++)
      if (path.Name[i] == '/')
	count++;
    return count;
  }

  static std::string GetFileName(const Path& path)
  {
    int index = path.Name.rfind('/');
    if (index != std::string::npos)
      return path.Name.substr(index + 1);
    else      
      return path;
  }

  static Path GetDirectoryName(const Path& path)
  {
    int index = path.Name.rfind('/');
    if (index != std::string::npos)
      return path.Name.substr(0, index);
    else      
      return Path("");
  }

  static Path Combine(const Path& first, const Path& second);

  operator std::string() const {
    return Name;
  }
  const char * c_str() const {
    return Name.c_str();
  }

  bool operator==(const Path& other) const {
    return Name == other.Name;
  }
  bool operator!=(const Path& other) const {
    return ! (*this == other);
  }
  Path& operator+=(const Path& other) {
    Name = Combine(Name, other.Name);
  }

  bool empty() const {
    return Name.empty();
  }

  std::string FileName() const {
    return GetFileName(Name);
  }

  Path DirectoryName() const {
    return Path(GetDirectoryName(Name));
  }
};

inline Path operator+(const Path& left, const Path& right) {
  return Path::Combine(left, right);
}

inline std::ostream& operator<<(std::ostream& out, const Path& path) {
  out << path.Name;
  return out;
}

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

  static md5sum_t checksum(const Path& path, md5sum_t& csum);

  friend std::ostream& operator<<(std::ostream& out, const md5sum_t& md5);
};

#define FILEINFO_NOFLAGS  0x00
#define FILEINFO_DIDSTAT  0x01
#define FILEINFO_READCSUM 0x02
#define FILEINFO_HANDLED  0x04

class Location;
class FileInfo
{
  mutable struct stat info;

public:
  mutable unsigned char flags;

  enum Kind {
    Collection, Directory, File, SymbolicLink, Special, Nonexistant, Last
  };

  std::string Name;
  Path	      FullName;
  Location *  Repository;
  Path	      Pathname;

  FileInfo(Location * _Repository = NULL);
  FileInfo(const Path& _FullName, FileInfo * _Parent = NULL,
	   Location * _Repository = NULL);
#if 0
  FileInfo(const FileInfo& other);
#endif
  ~FileInfo();

  void SetPath(const Path& _FullName);

  mutable Kind fileKind;

  void dostat() const;

public:
  void Reset() {
    fileKind = Nonexistant;
    flags    = FILEINFO_NOFLAGS;
  }

  Kind FileKind() const {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    return fileKind;
  }

  bool Exists() const {
    return FileKind() != Nonexistant;
  }

  off_t& Length() {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    assert(Exists());
    return info.st_size;
  }

  mode_t Permissions() {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    return info.st_mode & ~S_IFMT;
  }    
  void SetPermissions(mode_t mode) {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    info.st_mode = (((info.st_mode & S_IFMT) | mode) |
		    info.st_mode & ~S_IFMT);
  }    
  uid_t& OwnerId() {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    return info.st_uid;
  }    
  gid_t& GroupId() {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    return info.st_gid;
  }    

  struct timespec& LastWriteTime() {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    return info.st_mtimespec;
  }
  struct timespec& LastAccessTime() {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
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

  void CopyDetails(FileInfo& dest, bool dataOnly = false);
  void CopyAttributes(FileInfo& dest, bool dataOnly = false);
  void CopyAttributes(const Path& dest);

private:
  mutable md5sum_t csum;

public:
  md5sum_t& Checksum();
  md5sum_t  CurrentChecksum() const;
  FileInfo  LinkReference() const;

  void CreateDirectory();
  void Delete();

  typedef std::map<std::string, FileInfo *>  ChildrenMap;
  typedef std::pair<std::string, FileInfo *> ChildrenPair;

  void GetFileInfos(ChildrenMap& store) const;

  mutable FileInfo *	Parent;
  mutable ChildrenMap * Children;

  int ChildrenSize() const;

  ChildrenMap::iterator ChildrenBegin() const;
  ChildrenMap::iterator ChildrenEnd() const {
    assert(Children != NULL);
    return Children->end();
  }

  void       AddChild(FileInfo * entry);
  FileInfo * CreateChild(const std::string& name);
  void       DestroyChild(FileInfo * child);
  FileInfo * FindChild(const std::string& name);
  FileInfo * FindOrCreateChild(const std::string& name)
  {
    FileInfo * child = FindChild(name);
    if (child == NULL)
      child = CreateChild(name);
    return child;
  }

  FileInfo * FindMember(const Path& path);
  FileInfo * FindOrCreateMember(const Path& path);

  static FileInfo * ReadFrom(char *& data, FileInfo * parent = NULL,
			     Location * repository = NULL);

  void DumpTo(std::ostream& out, int depth = 0);
  void WriteTo(std::ostream& out);

  void PostAddChange(StateChangesMap& changesMap);
  void PostRemoveChange(FileInfo * ancestor, StateChangesMap& changesMap);
  void PostUpdateChange(FileInfo * ancestor, StateChangesMap& changesMap);
  void PostUpdatePropsChange(FileInfo * ancestor, StateChangesMap& changesMap);

  void CompareTo(FileInfo * ancestor, StateChangesMap& changesMap);
};

class File
{
public:
  static bool Exists(const Path& path) {
    FileInfo info(path);
    return info.Exists();
  }

  static void Delete(const Path& path) {
    if (unlink(path.c_str()) == -1)
      throw Exception("Failed to delete '" + path + "'");
  }

  static void Copy(const Path& path, const Path& dest);
  static void Move(const Path& path, const Path& dest)
  {
    if (rename(path.c_str(), dest.c_str()) == -1)
      throw Exception("Failed to move '" + path + "' to '" + dest + "'");
  }

  static void SetOwnership(const Path& path,
			   mode_t mode, uid_t uid, gid_t gid)
  {
    if (chmod(path.c_str(), mode) == -1)
      throw Exception("Failed to change permissions of '" + path + "'");
    if (chown(path.c_str(), uid, gid) == -1)
      throw Exception("Failed to change ownership of '" + path + "'");
  }

  static void SetAccessTimes(const Path& path,
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
      if (mkdir(info.Pathname.c_str(), 0755) == -1)
	throw Exception("Failed to create directory '" + info.Pathname + "'");
  }

public:
  static void CreateDirectory(const Path& path)
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

  static void Delete(const Path& path)
  {
    if (rmdir(path.c_str()) == -1)
      throw Exception("Failed to remove directory '" + path + "'");
  }

  static void Copy(const Path& path, const Path& dest)
  {
    assert(0);			// jww (2006-11-03): implement
  }

  static void Move(const Path& path, const Path& dest)
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

} // namespace Attic

#endif // _FILEINFO_H
