#ifndef _POSIX_H
#define _POSIX_H

#include "FileInfo.h"
#include "Broker.h"
#include "error.h"
#include "acconf.h"

#include <sys/stat.h>

namespace Attic {

class PosixFileInfo : public FileInfo
{
  typedef unsigned char posix_flags_t;

#define POSIX_FILEINFO_NOFLAGS	0x01
#define POSIX_FILEINFO_MODECHG	0x02
#define POSIX_FILEINFO_OWNRCHG	0x04
#define POSIX_FILEINFO_TIMECHG	0x08
#define POSIX_FILEINFO_LINKCHG	0x10
#define POSIX_FILEINFO_ALLFLAGS 0xff

  mutable struct stat	info;
  mutable posix_flags_t posixFlags;

  union {
    Path * LinkTargetPath;
  };

public:
  PosixFileInfo(Location * _Repository = NULL)
    : FileInfo(_Repository), posixFlags(POSIX_FILEINFO_NOFLAGS) {}
  
  PosixFileInfo(const Path& _FullName, FileInfo * _Parent = NULL,
		Location * _Repository = NULL)
    : FileInfo(_FullName, _Parent, _Repository),
      posixFlags(POSIX_FILEINFO_NOFLAGS) {}

  virtual ~PosixFileInfo() {}

  virtual Kind FileKind() const;

  bool IsSymbolicLink() const {
    return FileKind() == SymbolicLink;
  }

  virtual bool IsReadable() const;
  virtual bool IsWritable() const;
  virtual bool IsSearchable() const;

  virtual unsigned long long Length() const;

  mode_t Permissions() const;    
  void SetPermissions(const mode_t& mode);    

  const uid_t& OwnerId() const;    
  void SetOwnerId(const uid_t& uid) const;    

  const gid_t& GroupId() const;    
  void SetGroupId(const gid_t& gid);    

  virtual DateTime LastWriteTime() const;
  virtual void SetLastWriteTime(const DateTime& when);

  DateTime LastAccessTime() const;
  void SetLastAccessTime(const DateTime& when);

  Path LinkTarget() const;
  void SetLinkTarget(const Path& path);

  virtual bool CompareAttributes(const FileInfo& other) const;
  virtual void CopyAttributes(FileInfo& dest) const;
  virtual void Dump(std::ostream& out, bool verbose, int depth) const;

  friend class PosixVolumeBroker;
};

class PosixVolumeBroker : public VolumeBroker
{
  void SetPermissions(const Path& path, mode_t mode);
  void SetOwnership(const Path& path, uid_t uid, gid_t gid);
  void SetAccessTimes(const Path& path, const DateTime& LastAccessTime,
		      const DateTime& LastWriteTime);
  void SetLinkTarget(const Path& path, const Path& dest);

  void CreateFile(PosixFileInfo& entry);
  void DeleteFile(const Path& path);
  void CopyFile(const PosixFileInfo& entry, const Path& dest);
  void UpdateFile(const PosixFileInfo& entry, const PosixFileInfo& dest);
  void MoveFile(const PosixFileInfo& entry, const Path& dest);

  //void CreateDirectory(const PosixFileInfo& entry);
  void DeleteDirectory(const Path& entry);
  void CopyDirectory(const PosixFileInfo& entry, const Path& dest);
  void MoveDirectory(const PosixFileInfo& entry, const Path& dest);

public:
  explicit PosixVolumeBroker(const Path& _RootPath,
			     const Path& _VolumePath = "/")
    : VolumeBroker(_RootPath, _VolumePath) {}
    
  virtual FileInfo * FindRoot() {
    return CreateFileInfo("");
  }
  virtual FileInfo * CreateFileInfo(const Path& path,
				    FileInfo * parent = NULL) const {
    return new PosixFileInfo(path, parent, Repository);
  }

  virtual unsigned long long Length(const Path& path) const;

  virtual bool Exists(const Path& path) const;
  virtual bool IsReadable(const Path& path) const;
  virtual bool IsWritable(const Path& path) const;
  virtual bool IsSearchable(const Path& path) const;

  virtual void ReadAttributes(FileInfo& entry) const;
  virtual void SyncAttributes(const FileInfo& entry);
  virtual void CopyAttributes(const FileInfo& entry, const Path& dest);

  virtual void ComputeChecksum(const Path& path, md5sum_t& csum) const;

  virtual void ReadDirectory(FileInfo& entry) const;
  virtual void CreateDirectory(const Path& path);
  virtual void Create(FileInfo& entry);
  virtual void Delete(FileInfo& entry);
  virtual void Copy(const FileInfo& entry, const Path& dest);
  virtual void Move(FileInfo& entry, const Path& dest);

  virtual Path GetSignature(const FileInfo& entry) const { return Path(); }
  virtual Path CreateDelta(const FileInfo& entry, const Path& sigfile) { return Path(); }
  virtual void ApplyDelta(const FileInfo& entry, const Path& delta) {}
};

} // namespace Attic

#endif // _POSIX_H
