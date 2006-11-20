#include "FlatDB.h"
#include "Location.h"
#include "Posix.h"
#include "binary.h"

#include <fstream>

namespace Attic {

#define BINARY_VERSION 0x00000001L

FlatDatabaseBroker::~FlatDatabaseBroker()
{
  if (Dirty)
    Save(static_cast<FlatDBFileInfo *>(Repository->Root()));
}

FlatDBFileInfo * FlatDatabaseBroker::Load()
{
  PosixVolumeBroker broker("/");

  if (! broker.Exists(DatabasePath))
    return NULL;

  int    len  = broker.Length(DatabasePath);
  char * data = new char[len + 1];

  std::ifstream fin(DatabasePath.c_str());
  fin.read(data, len);
  if (fin.gcount() != len) {
    delete[] data;
    throw Exception("Failed to read database file '" + DatabasePath + "'");
  }

  FlatDBFileInfo * Root = NULL;
  try {
    char * ptr = data;
    if (read_binary_long<long>(ptr) == BINARY_VERSION) {
      Root = ReadFileInfo(ptr, NULL);
      //RegisterChecksums(Root);
    }
  }
  catch (...) {
    delete[] data;
    throw;
  }
  delete[] data;

  return Root;
}

FlatDBFileInfo *
FlatDatabaseBroker::ReadFileInfo(char *& data, FlatDBFileInfo * parent) const
{
  Path name;
  read_binary_string(data, name);

  FlatDBFileInfo * entry =
    static_cast<FlatDBFileInfo *>(CreateFileInfo(name, parent));

  read_binary_number(data, entry->fileKind);
  read_binary_number(data, entry->length);
  md5sum_t csum;
  read_binary_number(data, csum);
  entry->SetChecksum(csum);
  read_binary_number(data, entry->lastWriteTime);

  if (entry->IsDirectory()) {
    int children = read_binary_long<int>(data);
    for (int i = 0; i < children; i++)
      ReadFileInfo(data, entry);
  }
  return entry;
}

void FlatDatabaseBroker::Save(FlatDBFileInfo * Root)
{
  std::ofstream fout(DatabasePath.c_str());
  write_binary_long(fout, BINARY_VERSION);
  if (Root)
    WriteFileInfo(*Root, fout);
  fout.close();

  Dirty = false;
}

void FlatDatabaseBroker::WriteFileInfo(const FlatDBFileInfo& entry,
				       std::ostream& out) const
{
  write_binary_string(out, entry.Name);

  write_binary_number(out, entry.FileKind());
  write_binary_number(out, entry.Length());
  write_binary_number(out, entry.Checksum());
  write_binary_number(out, entry.LastWriteTime());

  if (entry.IsDirectory()) {
    write_binary_long(out, (int)entry.ChildrenSize());
    for (FileInfo::ChildrenMap::iterator i = entry.ChildrenBegin();
	 i != entry.ChildrenEnd();
	 i++)
      WriteFileInfo(static_cast<FlatDBFileInfo&>(*(*i).second), out);
  }
}

} // namespace Attic
