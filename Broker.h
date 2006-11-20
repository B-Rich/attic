#ifndef _BROKER_H
#define _BROKER_H

#include "FileInfo.h"

#include <string>

namespace Attic {

class Location;
class Broker
{
public:
  Location * Repository;

  virtual ~Broker() {}

  virtual void SetRepository(Location * _Repository) {
    Repository = _Repository;
  }

  virtual FileInfo * FindRoot() = 0;
  virtual FileInfo * CreateFileInfo(const Path& path,
				    FileInfo * parent = NULL) const = 0;

  virtual unsigned long long Length(const Path& path) const = 0;

  virtual bool Exists(const Path& path) const = 0;
  virtual bool IsReadable(const Path& path) const = 0;
  virtual bool IsWritable(const Path& path) const = 0;
  virtual bool IsSearchable(const Path& path) const = 0;

  virtual void ReadAttributes(FileInfo& entry) const = 0;
  virtual void SyncAttributes(const FileInfo& entry) = 0;
  virtual void CopyAttributes(const FileInfo& entry, const Path& dest) = 0;

  virtual void ComputeChecksum(const Path& path, md5sum_t& csum) const = 0;

  virtual void ReadDirectory(FileInfo& entry) const = 0;
  virtual void CreateDirectory(const Path& path) = 0;
  virtual void Create(FileInfo& entry) = 0;
  virtual void Delete(FileInfo& entry) = 0;
  virtual void Copy(const FileInfo& entry, const Path& dest) = 0;
  virtual void Move(FileInfo& entry, const Path& dest) = 0;

  virtual Path GetSignature(const FileInfo& entry) const = 0;
  virtual Path CreateDelta(const FileInfo& entry, const Path& sigfile) = 0;
  virtual void ApplyDelta(const FileInfo& entry, const Path& delta) = 0;

  virtual Path FullPath(const Path& subpath) const = 0;
  virtual std::string Moniker(const FileInfo& entry) const = 0;
};

class DatabaseBroker : public Broker {};

class Archive;
class VolumeBroker : public Broker
{
public:
  Path	       VolumePath;
  Path	       RootPath;
  Path	       CurrentPath;	// cache of VolumePath + RootPath
  unsigned int VolumeSize;
  unsigned int VolumeQuota;
  unsigned int MaximumSize;
  unsigned int MaximumPercent;
  Path         TempDirectory;

  std::string  CompressionScheme;
  std::string  CompressionOptions;
  std::string  EncryptionScheme;
  std::string  EncryptionOptions;

  Archive *    ArchivalStore;

  explicit VolumeBroker(const Path& _RootPath, const Path& _VolumePath = "/")
    : VolumePath(_VolumePath), RootPath(_RootPath),
      CurrentPath(Path::Combine(VolumePath, RootPath)),
      ArchivalStore(NULL) {}

  virtual Path FullPath(const Path& subpath) const {
    return Path::Combine(CurrentPath, subpath);
  }
  virtual std::string Moniker(const FileInfo& entry) const {
    return entry.Pathname;
  }
};

#if 0
class Socket;
class RemoteBroker : public VolumeBroker
{
public:
  std::string  Hostname;
  unsigned int PortNumber;
  std::string  Protocol;
  std::string  Username;
  std::string  Password;

private:
  Socket *     Connection;
};
#endif

} // namespace Attic

#endif // _BROKER_H
