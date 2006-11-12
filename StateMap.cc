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
      FileInfoArray entryArray;
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

FileInfoArray * StateMap::FindDuplicate(const FileInfo * item)
{
  if (item->IsRegularFile()) {
    ChecksumMap::iterator i = EntriesByChecksum.find(item->Checksum());
    if (i != EntriesByChecksum.end())
      return &(*i).second;
  }
  return NULL;
}

void StateMap::ApplyChange(const StateChange& change)
{
  FileInfo * entry;

  switch (change.ChangeKind) {
  case StateChange::Add:
    entry = Root->FindOrCreateMember(change.Item->FullName);
    change.Item->CopyDetails(*entry, true);
    change.Item->CopyAttributes(*entry, true);
    break;

  case StateChange::Remove:
    entry = Root->FindMember(change.Item->FullName);
    if (entry)
      entry->Parent->DestroyChild(entry);
    break;

  case StateChange::Update:
    entry = Root->FindMember(change.Item->FullName);
    if (entry)
      change.Item->CopyDetails(*entry, true);
    break;

  case StateChange::UpdateProps:
    entry = Root->FindMember(change.Item->FullName);
    if (entry)
      change.Item->CopyAttributes(*entry, true);
    break;

  default:
    assert(0);
    break;
  }
}

} // namespace Attic
