#ifndef _FILEINFO_H
#define _FILEINFO_H

#include "DateTime.h"

#include <string>
#include <map>

#include <sys/stat.h>

#include "error.h"
#include "md5.h"
#include "acconf.h"

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
#define FILEINFO_ALLFLAGS 0xff

class FileInfoData
{
protected:
  mutable unsigned char flags;
  mutable struct stat	info;
  mutable md5sum_t	csum;

public:
  typedef unsigned char flags_t;

  FileInfoData(flags_t _flags) : flags(_flags) {}

  void Reset() {
    flags = FILEINFO_NOFLAGS;
  }

  void SetFlags(flags_t _flags) {
    flags |= _flags;
  }
  void ClearFlags(flags_t _flags = FILEINFO_ALLFLAGS) {
    flags &= ~_flags;
  }
  bool HasFlags(flags_t _flags = FILEINFO_ALLFLAGS) const {
    return flags & _flags;
  }
};

class Location;
class FileInfo : public FileInfoData
{
public:
  typedef std::map<std::string, FileInfo *>  ChildrenMap;
  typedef std::pair<std::string, FileInfo *> ChildrenPair;

private:
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
    : FileInfoData(FILEINFO_DIDSTAT | FILEINFO_VIRTUAL),
      Repository(_Repository), Parent(NULL), Children(NULL) {
    info.st_mode |= S_IFDIR;
  }

  FileInfo(const Path& _FullName, FileInfo * _Parent = NULL,
	   Location * _Repository = NULL)
    : FileInfoData(FILEINFO_NOFLAGS),
      Repository(_Repository), Parent(_Parent), Children(NULL) {
    SetPath(_FullName);
    if (Parent) Parent->AddChild(this);
  }

  ~FileInfo();

  void SetPath(const Path& _FullName);

  bool Exists() const {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    return flags & FILEINFO_EXISTS;
  }

  const off_t& Length() const {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    assert(Exists());
    return info.st_size;
  }
  void SetLength(const off_t& len) {
    info.st_size = len;
  }

  const md5sum_t& Checksum() const;
  void SetChecksum(const md5sum_t& _csum) {
    csum = _csum;
  }

  mode_t Permissions() const {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    return info.st_mode & ~S_IFMT;
  }    
  void SetPermissions(const mode_t& mode) {
    info.st_mode = (((info.st_mode & S_IFMT) | mode) |
		    info.st_mode & ~S_IFMT);
  }    

  const uid_t& OwnerId() const {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    return info.st_uid;
  }    
  void SetOwnerId(const uid_t& uid) const {
    info.st_uid = uid;
  }    

  const gid_t& GroupId() const {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
    return info.st_gid;
  }    
  void SetGroupId(const gid_t& gid) {
    info.st_gid = gid;
  }    

  DateTime LastWriteTime() const {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
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
  void SetLastWriteTime(const DateTime& when) {
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
  }

  DateTime LastAccessTime() const {
    if (! (flags & FILEINFO_DIDSTAT)) dostat();
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
  void SetLastAccessTime(const DateTime& when) {
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

  md5sum_t CurrentChecksum() const;
  FileInfo Directory() const;
  FileInfo LinkReference() const;

  void	   CreateDirectory();
  void	   Delete();
  void	   Copy(const Path& dest);
  void	   Move(const Path& dest);
  void	   CopyDetails(FileInfo& dest, bool dataOnly = false);
  void	   CopyAttributes(FileInfo& dest, bool dataOnly = false);
  void	   CopyAttributes(const Path& dest);
  void     GetFileInfos(ChildrenMap& store) const;
  int	   ChildrenSize() const;

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

  void WriteTo(std::ostream& out) const;
  void DumpTo(std::ostream& out, bool verbose, int depth = 0);
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
			     const DateTime& LastAccessTime,
			     const DateTime& LastWriteTime);
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

} // namespace Attic

#endif // _FILEINFO_H
