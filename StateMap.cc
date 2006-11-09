#include "StateMap.h"
#include "binary.h"

#include <fstream>
#include <iostream>

#define BINARY_VERSION 0x00000001L

namespace Attic {

void StateMap::RegisterChecksums(FileInfo * entry)
{
  if (entry->IsRegularFile())
    EntriesByChecksum[entry->Checksum()] = entry;
  else if (entry->IsDirectory())
    for (FileInfo::ChildrenMap::const_iterator i = entry->ChildrenBegin();
	 i != entry->ChildrenEnd();
	 i++)
      RegisterChecksums((*i).second);
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

FileInfo * StateMap::FindDuplicate(FileInfo * item)
{
  FileInfo * foundEntry = NULL;

  if (item->FileKind() == FileInfo::File) {
    ChecksumMap::iterator i = EntriesByChecksum.find(item->Checksum());
    if (i != EntriesByChecksum.end())
      foundEntry = (*i).second;
  }
  return foundEntry;
}

void StateMap::CompareTo(const StateMap * ancestor,
			 StateChangesMap& changesMap) const
{
  Root->CompareTo(ancestor->Root, changesMap);
}

#if 0

StateMap * StateMap::ReadState(const std::deque<std::string>& paths,
				 const std::deque<Regex *>& IgnoreList,
				 bool verboseOutput)
{
  StateMap * state = new StateMap();

  FileInfo info;
  state->Root = new FileInfo(state, NULL, info);

  for (std::deque<std::string>::const_iterator i = paths.begin();
       i != paths.end();
       i++)
    state->ReadEntry(state->Root, *i, Path::GetFileName(*i), IgnoreList,
		     verboseOutput);

  return state;
}
  
StateMap * StateMap::ReadState(const std::string& path,
				 const std::deque<Regex *>& IgnoreList,
				 bool verboseOutput)
{
  StateMap * state = new StateMap();

  FileInfo info(path);
  char * data = new char[info.Length() + 1];
  std::ifstream fin(path.c_str());
  fin.read(data, info.Length());
  if (fin.gcount() != info.Length()) {
    delete[] data;
    throw Exception("Failed to read database file '" + path + "'");
  }

  try {
    char * ptr = data;
    state->Root = state->LoadElements(ptr, NULL);
  }
  catch (...) {
    delete[] data;
    throw;
  }
  delete[] data;

  return state;
}

void StateMap::BackupEntry(FileInfo * entry)
{
#if 0
  if (GenerationPath == NULL)
    return;

  FileInfo target(Path::Combine(GenerationPath->FullName,
				entry->Info.RelativeName));
  target.CreateDirectory();
  entry->Copy(target.FullName);
#endif
}

void StateMap::MergeChanges(StateMap * other)
{
  // Look for any conflicting changes between this object's change
  // set and other's change set.

  for (std::vector<StateChange *>::iterator i = other->Changes.begin();
       i != other->Changes.end();
       i++)
    Changes.push_back(*i);
}

void StateMap::PerformChanges()
{
  std::stable_sort(Changes.begin(), Changes.end(), StateChangeComparer());

  for (std::vector<StateChange *>::iterator i = Changes.begin();
       i != Changes.end();
       i++) {
    if (dynamic_cast<CreateChange *>(*i) != NULL) {
      if (DebugMode)
	(*i)->Identify();
      (*i)->Report();
      (*i)->Perform();
    }
  }

  for (std::vector<StateChange *>::iterator i = Changes.begin();
       i != Changes.end();
       i++) {
    if (DebugMode) {
      std::cout << "Preparing: ";
      (*i)->Identify();
    }
    (*i)->Prepare();
  }

  for (std::vector<StateChange *>::iterator i = Changes.begin();
       i != Changes.end();
       i++) {
    if (dynamic_cast<CreateChange *>(*i) == NULL) {
      if (DebugMode)
	(*i)->Identify();
      (*i)->Report();
      (*i)->Perform();
    }
  }

  Changes.clear();
}

void StateMap::ReportChanges()
{
  std::stable_sort(Changes.begin(), Changes.end(), StateChangeComparer());

  for (std::vector<StateChange *>::iterator i = Changes.begin();
       i != Changes.end();
       i++)
    (*i)->Report();
}

void StateMap::WriteTo(const std::string& path)
{
  FileInfo info(path);

  info.CreateDirectory();
  if (info.Exists())
    info.Delete();

  std::ofstream fout(path.c_str());
  Root->WriteTo(fout, true);
}

FileInfo * StateMap::ReadEntry(FileInfo *		    parent,
				  const std::string&	    path,
				  const std::string&	    relativePath,
				  const std::deque<Regex *>& IgnoreList,
				  bool			    verboseOutput)
{
  for (std::deque<Regex *>::const_iterator i = IgnoreList.begin();
       i != IgnoreList.end();
       i++)
    if ((*i)->IsMatch(path))
      return NULL;

  FileInfo * entry =
    new FileInfo(this, parent, FileInfo(path, relativePath));

  if (entry->Info.FileKind() == FileInfo::Directory)
    ReadDirectory(entry, path, relativePath, IgnoreList, verboseOutput);

  return entry;
}

void StateMap::ReadDirectory(FileInfo *		entry,
			      const std::string&	path,
			      const std::string&	relativePath,
			      const std::deque<Regex *>& IgnoreList,
			      bool			verboseOutput)
{
  if (verboseOutput && Path::Parts(relativePath) < 4)
    std::cout << "Scanning: " << entry->Info.FullName << std::endl;

  try {
    std::deque<FileInfo> infos;
    entry->Info.GetFileInfos(infos);
    for (std::deque<FileInfo>::iterator i = infos.begin();
	 i != infos.end();
	 i++)
      ReadEntry(entry, (*i).FullName, (*i).RelativeName,
		IgnoreList, verboseOutput);
  }
  catch (...) {
    // Defend against access violations and failures to read!
  }
}

StateMap * StateMap::CreateDatabase(const std::string& path,
				      const std::string& dbfile,
				      bool verbose)
{
  std::deque<Regex *> ignoreList;
  std::deque<std::string> collection;
  collection.push_back(path);

  StateMap * tree = StateMap::ReadState(collection, ignoreList, verbose);
  if (! dbfile.empty())
    tree->WriteTo(dbfile);

  if (DebugMode) {
    std::cout << "created database state:";
    tree->Report();
  }
  return tree;
}

#endif

} // namespace Attic
