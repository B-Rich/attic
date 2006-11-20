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
#if 0
  if (! File::Exists(DatabasePath))
    return;

  int    len  = File::Length(DatabasePath);
  char * data = new char[len + 1];

  std::ifstream fin(DatabasePath.c_str());
  fin.read(data, len);
  if (fin.gcount() != len) {
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
#endif
}

void DatabaseBroker::Save()
{
#if 0
  std::ofstream fout(DatabasePath.c_str());
  write_binary_long(fout, BINARY_VERSION);
  if (Root)
    Root->WriteTo(fout);
  fout.close();

  Dirty = false;
#endif
}

void DatabaseBroker::Dump(std::ostream& out, bool verbose)
{
  if (Root)
    Root->Dump(out, verbose);
}

#if 0
FileInfo * FileInfo::ReadFrom(char *& data, FileInfo * parent,
			      Location * repository)
{
  FileInfo * entry = new FileInfo(repository);

  read_binary_string(data, entry->Name);
  read_binary_number(data, static_cast<FileInfoData&>(*entry));

  entry->Parent = parent;
  if (parent) {
    entry->SetPath(Path::Combine(parent->FullName, entry->Name));
    parent->AddChild(entry);
  } else {
    entry->SetPath(entry->Name);
  }

  if (entry->IsDirectory()) {
    int children = read_binary_long<int>(data);
    for (int i = 0; i < children; i++)
      ReadFrom(data, entry, repository);
  }
  return entry;
}

void FileInfo::WriteTo(std::ostream& out) const
{
  write_binary_string(out, Name);
  write_binary_number(out, static_cast<const FileInfoData&>(*this));

  if (IsDirectory()) {
    write_binary_long(out, (int)ChildrenSize());
    for (ChildrenMap::iterator i = ChildrenBegin();
	 i != ChildrenEnd();
	 i++)
      (*i).second->WriteTo(out);
  }
}
#endif

} // namespace Attic
