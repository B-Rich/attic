#ifndef _FLATDB_H
#define _FLATDB_H

#include "Broker.h"

namespace Attic {

class FlatDBFileInfo : public FileInfo
{
private:
  unsigned long long length;

  Kind     fileKind;
  DateTime lastWriteTime;

public:
  FlatDBFileInfo(Location * _Repository = NULL)
    : FileInfo(_Repository) {}
  
  FlatDBFileInfo(const Path& _FullName, FileInfo * _Parent = NULL,
		 Location * _Repository = NULL)
    : FileInfo(_FullName, _Parent, _Repository) {}

  virtual unsigned long long Length() const {
    return length;
  }

  virtual DateTime LastWriteTime() const {
    return lastWriteTime;
  }
  virtual void SetLastWriteTime(const DateTime& when) {
    lastWriteTime = when;
  }

  virtual Kind FileKind() const {
    return fileKind;
  }

  virtual void WriteData(std::ostream& out) const {
    assert(0);
  }
  virtual void ReadData(std::istream& in) {
    assert(0);
  }

  virtual void Copy(const FileInfo& source) {}
  virtual void CopyAttributes(const FileInfo& source) {}

  virtual bool CompareAttributes(const FileInfo& other) const {
    return false;
  }

  virtual void Dump(std::ostream& out, bool verbose, int depth = 0) const {}

  friend class FlatDatabaseBroker;
};

class FlatDatabaseBroker : public DatabaseBroker
{
public:
  Path DatabasePath;
  bool Dirty;

  FlatDatabaseBroker(const Path& _DatabasePath)
    : DatabasePath(_DatabasePath), Dirty(false) {}

  virtual ~FlatDatabaseBroker();

  virtual FileInfo * FindRoot() {
    return Load();
  }
  virtual FileInfo * CreateFileInfo(const Path& path,
				    FileInfo * parent = NULL) const {
    return new FlatDBFileInfo(path, parent, Repository);
  }

  virtual long long unsigned int Length(const Path&) const {
    return 0;
  }
  virtual bool Exists(const Path&) const {
    assert(0); return false;
  }
  virtual bool IsReadable(const Path&) const {
    assert(0); return false;
  }
  virtual bool IsWritable(const Path&) const {
    assert(0); return false;
  }
  virtual bool IsSearchable(const Path&) const {
    assert(0); return false;
  }
  virtual void ReadAttributes(FileInfo&) const {
    return;
  }
  virtual void SyncAttributes(const FileInfo&) {
    assert(0);
  }
  virtual void CopyAttributes(const FileInfo&, const Path&) {
    assert(0);
  }
  virtual void ComputeChecksum(const Path&, md5sum_t&) const {
    assert(0);
  }
  virtual void ReadDirectory(FileInfo&) const {
    assert(0);
  }
  virtual void CreateDirectory(const Path&) {
    assert(0);
  }
  virtual void Create(FileInfo&);
  virtual void Delete(FileInfo&) {
    assert(0);
  }
  virtual void Copy(const FileInfo&, const Path&) {
    assert(0);
  }
  virtual void Move(FileInfo&, const Path&) {
    assert(0);
  }
  virtual Path GetSignature(const FileInfo&) const {
    assert(0); return Path();
  }
  virtual Path CreateDelta(const FileInfo&, const Path&) {
    assert(0); return Path();
  }
  virtual void ApplyDelta(const FileInfo&, const Path&) {
    assert(0);
  }
  virtual Path FullPath(const Path& path) const {
    return path;
  }
  virtual std::string Moniker(const FileInfo& entry) const {
    return "flatdb://" + DatabasePath + "/" + entry.FullName;
  }

private:
  FlatDBFileInfo * Load();
  FlatDBFileInfo * ReadFileInfo(char *& entry, FlatDBFileInfo * parent) const;

  void Save(FlatDBFileInfo * Root);
  void WriteFileInfo(const FlatDBFileInfo& entry, std::ostream& out) const;
};

} // namespace Attic

#endif // _FLATDB_H
