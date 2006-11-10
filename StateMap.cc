#include "StateMap.h"
#include "binary.h"

#include <fstream>
#include <iostream>

#define BINARY_VERSION 0x00000001L

namespace Attic {

void StateMap::RegisterChecksums(FileInfo * entry)
{
  if (entry->IsRegularFile()) {
    ChecksumMap::iterator i = EntriesByChecksum.find(entry->Checksum());
    if (i == EntriesByChecksum.end()) {
      std::deque<FileInfo *> entryArray;
      entryArray.push_back(entry);
      EntriesByChecksum.insert(ChecksumPair(entry->Checksum(), entryArray));
    } else {
      (*i).second.push_back(entry);
    }
  }
  else if (entry->IsDirectory()) {
    for (FileInfo::ChildrenMap::const_iterator i = entry->ChildrenBegin();
	 i != entry->ChildrenEnd();
	 i++)
      RegisterChecksums((*i).second);
  }
}

void StateMap::LoadFrom(const Path& path)
{
  FileInfo info(path);
  if (! info.Exists())
    return;

  char * data = new char[info.Length() + 1];

  std::ifstream fin(path.c_str());
  fin.read(data, info.Length());
  if (fin.gcount() != info.Length()) {
    delete[] data;
    throw Exception("Failed to read database file '" + path + "'");
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

void StateMap::SaveTo(std::ostream& out)
{
  write_binary_long(out, BINARY_VERSION);
  if (Root)
    Root->WriteTo(out);
}

std::deque<FileInfo *> * StateMap::FindDuplicate(FileInfo * item)
{
  if (item->IsRegularFile()) {
    ChecksumMap::iterator i = EntriesByChecksum.find(item->Checksum());
    if (i != EntriesByChecksum.end())
      return &(*i).second;
  }
  return NULL;
}

} // namespace Attic
