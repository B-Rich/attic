#include "Broker.h"
#include "binary.h"

#include <fstream>

#include <errno.h>

namespace Attic {

#define BINARY_VERSION 0x00000001L

DatabaseBroker::~DatabaseBroker()
{
  Sync();
  delete Root;
}

void DatabaseBroker::Sync()
{
  if (Dirty)
    Save();
}

void DatabaseBroker::Load()
{
  FileInfo info(DatabasePath, NULL, Repository);
  if (! info.Exists())
    return;

  char * data = new char[info.Length() + 1];

  std::ifstream fin(DatabasePath.c_str());
  fin.read(data, info.Length());
  if (fin.gcount() != info.Length()) {
    delete[] data;
    throw Exception("Failed to read database file '" + DatabasePath + "'");
  }

  try {
    char * ptr = data;
    if (read_binary_long<long>(ptr) == BINARY_VERSION) {
      Root = FileInfo::ReadFrom(ptr);
      RegisterChecksums(Root);
    }
  }
  catch (...) {
    delete[] data;
    throw;
  }
  delete[] data;
}

void DatabaseBroker::Save()
{
  std::ofstream fout(DatabasePath.c_str());
  write_binary_long(fout, BINARY_VERSION);
  if (Root)
    Root->WriteTo(fout);
  fout.close();

  Dirty = false;
}

void DatabaseBroker::Dump(std::ostream& out, bool verbose)
{
  if (Root)
    Root->DumpTo(out, verbose);
}

void VolumeBroker::ReadFileProperties(const Path& subpath, FileInfo * file)
{
  Path Pathname(Path::Combine(CurrentPath, subpath));

  if (lstat(Pathname.c_str(), &file->info) == -1) {
    if (errno == ENOENT) {
      file->flags |= FILEINFO_DIDSTAT;
      return;
    }
    throw Exception("Failed to stat '" + Pathname + "'");
  }
  file->flags |= FILEINFO_DIDSTAT | FILEINFO_EXISTS;
}

} // namespace Attic
