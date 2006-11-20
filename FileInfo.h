#ifndef _FILEINFO_H
#define _FILEINFO_H

#include "Path.h"
#include "DateTime.h"

#include <string>
#include <iostream>
#include <map>
#include <deque>

#include <assert.h>

#include "error.h"
#include "md5.h"

namespace Attic {

class md5sum_t
{
public:
  md5_byte_t digest[16];

  bool operator==(const md5sum_t& other) const {
    return memcmp(digest, other.digest, sizeof(md5_byte_t) * 16) == 0;
  }
  bool operator!=(const md5sum_t& other) const {
    return ! (*this == other);
  }
  bool operator<(const md5sum_t& other) const {
    return memcmp(digest, other.digest, sizeof(md5_byte_t) * 16) < 0;
  }

  friend inline std::ostream& operator<<(std::ostream& out, const md5sum_t& md5) {
    for (int i = 0; i < 16; i++) {
      out.fill('0');
      out << std::hex << (int)md5.digest[i];
    }
    return out;
  }
};

#define FILEINFO_NOFLAGS   0x00
#define FILEINFO_EXISTS	   0x01 // file exists on disk
#define FILEINFO_READATTR  0x02 // attributes have been read
#define FILEINFO_READCSUM  0x04 // checksum has been computed
#define FILEINFO_VIRTUAL   0x08 // file is a virtual directory
#define FILEINFO_TEMPFILE  0x10 // delete upon destruction
#define FILEINFO_HANDLED   0x20
#define FILEINFO_ALLFLAGS  0xff

class Location;
class FileInfo
{
public:
  typedef std::map<std::string, FileInfo *>  ChildrenMap;
  typedef std::pair<std::string, FileInfo *> ChildrenPair;
  typedef unsigned char			     flags_t;

  enum Kind {
    Nonexistant,
    RegularFile,
    Directory,
    SymbolicLink,
    CharDevice,
    BlockDevice,
    NamedPipe,
    Socket,
    Alias,
    Special = 0xff
  };

  typedef std::map<std::string, void *>  AttributesMap;
  typedef std::pair<std::string, void *> AttributesPair;

protected:
  mutable flags_t	  flags;
  mutable md5sum_t	  csum;
  mutable ChildrenMap *	  Children; // This is dynamically generated on-demand
  mutable AttributesMap * Attributes; // This is only created if necessary

public:
  Location *  Repository;
  FileInfo *  Parent;		// This is computed during load/read
  Path	      FullName;		// This is computed during load/read
  std::string Name;
  Path        Pathname;		// The current literal location on disk

  explicit FileInfo(Location * _Repository = NULL)
    : flags(FILEINFO_READATTR | FILEINFO_VIRTUAL),
      Children(NULL), Repository(_Repository), Parent(NULL) {}

  explicit FileInfo(const Path& _FullName, FileInfo * _Parent = NULL,
		    Location * _Repository = NULL)
    : flags(FILEINFO_NOFLAGS),
      Children(NULL), Repository(NULL), Parent(NULL) {
    SetDetails(_FullName, _Parent, _Repository);
  }

  virtual ~FileInfo();

  void SetDetails(const Path& _FullName, FileInfo * _Parent = NULL,
		  Location * _Repository = NULL);

  virtual void Reset() {
    flags = FILEINFO_NOFLAGS;
  }
  flags_t Flags() const {
    return flags;
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

  const md5sum_t& Checksum() const;
  void SetChecksum(const md5sum_t& _csum) {
    csum = _csum;
    SetFlags(FILEINFO_READCSUM);
  }
  md5sum_t CurrentChecksum() const;

  void * GetAttribute(const std::string& name) const;
  void SetAttribute(const std::string& name, void * data);

  virtual void ClearAttribute(const std::string& name) {
    Attributes->erase(name);
  }
  virtual void ClearAllAttributes(const std::string& name) {
    Attributes->clear();
  }

  virtual DateTime LastWriteTime() const = 0;
  virtual void SetLastWriteTime(const DateTime& when) = 0;

  void ReadAttributes();
  bool Exists() const;

  virtual Kind FileKind() const = 0;

  bool IsRegularFile() const { return FileKind() == RegularFile; }
  bool IsDirectory() const   { return FileKind() == Directory; }
  bool IsVirtual() const     { return HasFlags(FILEINFO_VIRTUAL); }
  bool IsTempFile() const    { return HasFlags(FILEINFO_TEMPFILE); }

  virtual unsigned long long Length() const = 0;

  Path DirectoryName() const {
    return FullName.DirectoryName();
  }
  void Create();
  void CreateDirectory() const;
  void Delete();
  void Copy(const Path& dest) const;
  void Move(const Path& dest);
  void CopyAttributes(const Path& dest) const;

  virtual void CopyAttributes(FileInfo& dest) const = 0;
  virtual bool CompareAttributes(const FileInfo& other) const = 0;
  virtual void Dump(std::ostream& out, bool verbose, int depth = 0) const = 0;

  ChildrenMap::iterator ChildrenBegin() const;
  ChildrenMap::iterator ChildrenEnd() const {
    assert(Children != NULL);
    return Children->end();
  }
  ChildrenMap::size_type ChildrenSize() const;

  FileInfo *  CreateChild(const std::string& name);
  void        InsertChild(FileInfo * entry);
  void        RemoveChild(FileInfo * child);
  FileInfo *  FindChild(const std::string& name);
  FileInfo *  FindOrCreateChild(const std::string& name);
  FileInfo *  FindMember(const Path& path);
  FileInfo *  FindOrCreateMember(const Path& path);

  std::string Moniker() const;
};

typedef std::deque<FileInfo *> FileInfoArray;

} // namespace Attic

#endif // _FILEINFO_H
