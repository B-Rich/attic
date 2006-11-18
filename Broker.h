#ifndef _BROKER_H
#define _BROKER_H

#include "FileInfo.h"

#include <string>

namespace Attic {

class Broker
{
public:
  Location * Repository;

  virtual void Initialize(Location * _Repository) {
    Repository = _Repository;
  }
  virtual void Sync() {}
  virtual void GetSignature() {}
  virtual void ApplyDelta() {}
  virtual void SearchContents() {}
  virtual void ReadDirectory(const Path& subpath, FileInfo * dir) {}
  virtual void DeleteFile(const Path& subpath) {}
  virtual void InstallFile(const FileInfo * file, const Path& subpath) {}
  virtual void ReadFileProperties(const Path& subpath, FileInfo * file) {}
  virtual void UpdateFileProperties(const FileInfo * file,
				    const Path& subpath) {}

  virtual void RegisterChecksums(const FileInfo * root) {}
  virtual md5sum_t ComputeChecksum(const Path& subpath) {}

  virtual std::string Moniker(const Path& subpath) {
    return subpath;
  }
};

class DatabaseBroker : public Broker
{
  FileInfo *  Root;

public:
  Path        DatabasePath;
  std::string Username;
  std::string Password;
  bool        Dirty;

  DatabaseBroker(const Path& _DatabasePath)
    : DatabasePath(_DatabasePath), Dirty(false) {}
  virtual ~DatabaseBroker();

  virtual void Sync();

  void Load();
  void Save();

  void Dump(std::ostream& out, bool verbose);
};

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

  VolumeBroker(const Path& _RootPath, const Path& _VolumePath = "/")
    : VolumePath(_VolumePath), RootPath(_RootPath),
      CurrentPath(Path::Combine(VolumePath, RootPath)),
      ArchivalStore(NULL) {}

  virtual void ReadFileProperties(const Path& subpath, FileInfo * file);
};

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

} // namespace Attic

#endif // _BROKER_H
