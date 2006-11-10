#ifndef _FILEINFO_H
#define _FILEINFO_H

#include "DateTime.h"

#include <string>
#include <map>

#include <sys/stat.h>

#include "error.h"
#include "md5.h"

namespace Attic {

class Path : public std::string
{
public:
  Path() {}
  Path(const char * name) : std::string(name) {}
  Path(const std::string& name) : std::string(name) {}

  static Path ExpandPath(const Path& path);
  static Path Combine(const Path& first, const Path& second);

#if 0
  Path& operator+=(const Path& other) {
    *this = Combine(*this, other);
    return *this;
  }
#endif

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
#define FILEINFO_VIRTUAL  0x08
#define FILEINFO_EXISTS   0x10

class Location;
class FileInfo
{
public:
  typedef std::map<std::string, FileInfo *>  ChildrenMap;
  typedef std::pair<std::string, FileInfo *> ChildrenPair;

private:
  mutable unsigned char flags;
  mutable struct stat	info;
  mutable md5sum_t	csum;
  mutable ChildrenMap * Children; // This is dynamically generated on-demand

  void dostat() const;

public:
  std::string Name;
  Path	      FullName;		// This is computed during load/read
  Location *  Repository;
  Path	      Pathname;		// cache of Repository->CurrentPath + FullName
  FileInfo *  Parent;		// This is computed during load/read

public:
  FileInfo(Location * _Repository = NULL)
    : flags(FILEINFO_DIDSTAT | FILEINFO_VIRTUAL),
      Repository(_Repository), Parent(NULL), Children(NULL) {
    info.st_mode |= S_IFDIR;
  }

  FileInfo::FileInfo(const Path& _FullName, FileInfo * _Parent = NULL,
		     Location * _Repository = NULL)
    : flags(FILEINFO_NOFLAGS),
      Repository(_Repository), Parent(_Parent), Children(NULL) {
    SetPath(_FullName);
    if (Parent) Parent->AddChild(this);
  }

  ~FileInfo();

  void SetPath(const Path& _FullName);
  void Reset() {
    flags = FILEINFO_NOFLAGS;
  }

  void SetFlags(unsigned char _flags) {
    flags |= _flags;
  }
  void ClearFlags(unsigned char _flags) {
    flags &= ~_flags;
  }
  bool HasFlags(unsigned char _flags) const {
    return flags & _flags;
  }

  bool Exists() const {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    return flags & FILEINFO_EXISTS;
  }

  off_t& Length() const {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    assert(Exists());
    return info.st_size;
  }

  mode_t Permissions() const {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    return info.st_mode & ~S_IFMT;
  }    
  void SetPermissions(mode_t mode) {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    info.st_mode = (((info.st_mode & S_IFMT) | mode) |
		    info.st_mode & ~S_IFMT);
  }    

  uid_t& OwnerId() const {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    return info.st_uid;
  }    
  gid_t& GroupId() const {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    return info.st_gid;
  }    

  struct timespec& LastWriteTime() const {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    return info.st_mtimespec;
  }
  struct timespec& LastAccessTime() const {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    return info.st_atimespec;
  }

  int FileKind() const {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    return info.st_mode & S_IFMT;
  }

  bool IsVirtual() const {
    return flags & FILEINFO_VIRTUAL;
  }
  bool IsDirectory() const {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    return (info.st_mode & S_IFMT) == S_IFDIR;
  }
  bool IsSymbolicLink() const {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    return (info.st_mode & S_IFMT) == S_IFLNK;
  }
  bool IsRegularFile() const {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    return (info.st_mode & S_IFMT) == S_IFREG;
  }

  md5sum_t& Checksum() const;
  md5sum_t  CurrentChecksum() const;
  FileInfo  Directory() const;
  FileInfo  LinkReference() const;
  void	    CreateDirectory();
  void	    Delete();
  void	    Copy(const Path& dest);
  void	    Move(const Path& dest);
  void	    CopyDetails(FileInfo& dest, bool dataOnly = false);
  void	    CopyAttributes(FileInfo& dest, bool dataOnly = false);
  void	    CopyAttributes(const Path& dest);
  void      GetFileInfos(ChildrenMap& store) const;
  int	    ChildrenSize() const;

  ChildrenMap::iterator ChildrenBegin() const;
  ChildrenMap::iterator ChildrenEnd() const {
    assert(Children != NULL);
    return Children->end();
  }

  void       AddChild(FileInfo * entry);
  FileInfo * CreateChild(const std::string& name);
  void       DestroyChild(FileInfo * child);
  FileInfo * FindChild(const std::string& name);

  FileInfo * FindOrCreateChild(const std::string& name) {
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
};

class File
{
public:
  static bool Exists(const Path& path) {
    return FileInfo(path).Exists();
  }

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
  static void CreateDirectory(const FileInfo& info) {
    if (! info.Exists())
      if (mkdir(info.Pathname.c_str(), 0755) == -1)
	throw Exception("Failed to create directory '" + info.Pathname + "'");
  }

public:
  static void CreateDirectory(const Path& path);
  static void Delete(const Path& path) {
    if (rmdir(path.c_str()) == -1)
      throw Exception("Failed to remove directory '" + path + "'");
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
