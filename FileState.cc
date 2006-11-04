#include "FileState.h"

#include <fstream>
#include <iostream>

namespace Attic {

void FileState::RegisterUpdate(StateEntry * baseEntryToUpdate,
			       StateEntry * entryToUpdate,
			       StateEntry * entryToUpdateFrom,
			       const std::string& reason)
{
  Changes.push_back(new ObjectUpdate(false, entryToUpdate,
				     entryToUpdateFrom, reason));

  if (baseEntryToUpdate != entryToUpdate)
    baseEntryToUpdate->FileStateObj->Changes.push_back
      (new ObjectUpdate(true, baseEntryToUpdate, entryToUpdateFrom, reason));
}

void FileState::RegisterUpdateProps(StateEntry * baseEntryToUpdate,
				    StateEntry * entryToUpdate,
				    StateEntry * entryToUpdateFrom,
				    const std::string& reason)
{
  if (entryToUpdate == entryToUpdateFrom)
    return;

  Changes.push_back(new ObjectUpdateProps(false, entryToUpdate,
					  entryToUpdateFrom, reason));

  if (baseEntryToUpdate != entryToUpdate)
    baseEntryToUpdate->FileStateObj->Changes.push_back
      (new ObjectUpdateProps(true, baseEntryToUpdate, entryToUpdateFrom, reason));
}

void FileState::RegisterDelete(StateEntry * baseEntry, StateEntry * entry)
{
  if (entry->Info.IsDirectory())
    for (StateEntry::children_map::const_iterator i = entry->Children.begin();
	 i != entry->Children.end();
	 i++) {
      StateEntry * baseChild =
	(baseEntry != entry ?
	 baseEntry->FindChild((*i).first) : (*i).second);
      RegisterDelete(baseChild, (*i).second);
    }

  Changes.push_back(new ObjectDelete(false, entry));
  if (baseEntry != entry)
    baseEntry->FileStateObj->Changes.push_back(new ObjectDelete(true, baseEntry));
}

void FileState::RegisterCopy(StateEntry * entry, StateEntry * baseDirToCopyTo,
			     StateEntry * dirToCopyTo)
{
  if (entry->Info.IsDirectory()) {
    StateEntry * baseSubDir = NULL;
    StateEntry * subDir     = NULL;
    for (StateEntry::children_map::const_iterator i = entry->Children.begin();
	 i != entry->Children.end();
	 i++) {
      if (subDir == NULL) {
	baseSubDir = subDir =
	  dirToCopyTo->FindOrCreateChild(entry->Info.Name);
	Changes.push_back(new ObjectCreate(false, subDir, entry));

	if (baseDirToCopyTo != dirToCopyTo) {
	  baseSubDir = baseDirToCopyTo->FindOrCreateChild(entry->Info.Name);
	  baseDirToCopyTo->FileStateObj->Changes.push_back
	    (new ObjectCreate(true, baseSubDir, entry));
	}
      }
      RegisterCopy((*i).second, baseSubDir, subDir);
    }
    if (subDir != NULL) {
      Changes.push_back(new ObjectUpdateProps(false, subDir, entry));
      if (baseSubDir != subDir)
	baseSubDir->FileStateObj->Changes.push_back
	  (new ObjectUpdateProps(true, baseSubDir, entry));
    }
  } else {
    Changes.push_back(new ObjectCopy(false, entry, dirToCopyTo));
    if (baseDirToCopyTo != dirToCopyTo)
      baseDirToCopyTo->FileStateObj->Changes.push_back
	(new ObjectCopy(true, entry, baseDirToCopyTo));
  }
}

StateEntry * FileState::FindDuplicate(StateEntry * child)
{
  StateEntry * foundEntry = NULL;

  if (! StateEntry::TrustMode && child->Info.FileKind() == FileInfo::File) {
    checksum_map::iterator i = ChecksumDict.find(child->Info.Checksum());
    if (i != ChecksumDict.end())
      foundEntry = (*i).second;
  }

  return foundEntry;
}

FileState * FileState::ReadState(const std::list<std::string>& paths,
				 const std::list<Regex *>& IgnoreList,
				 bool verboseOutput)
{
  FileState * state = new FileState();

  FileInfo info;
  state->Root = new StateEntry(state, NULL, info);

  for (std::list<std::string>::const_iterator i = paths.begin();
       i != paths.end();
       i++)
    state->ReadEntry(state->Root, *i, Path::GetFileName(*i), IgnoreList,
		     verboseOutput);

  return state;
}
  
FileState * FileState::ReadState(const std::string& path,
				 const std::list<Regex *>& IgnoreList,
				 bool verboseOutput)
{
  FileState * state = new FileState();

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

void FileState::BackupEntry(StateEntry * entry)
{
  if (GenerationPath == NULL)
    return;

  FileInfo target(Path::Combine(GenerationPath->FullName,
				entry->Info.RelativeName));
  target.CreateDirectory();
  entry->Copy(target.FullName);
}

void FileState::MergeChanges(FileState * other)
{
  // Look for any conflicting changes between this object's change
  // set and other's change set.

  for (std::vector<StateChange *>::iterator i = other->Changes.begin();
       i != other->Changes.end();
       i++)
    Changes.push_back(*i);
}

void FileState::PerformChanges()
{
  std::stable_sort(Changes.begin(), Changes.end(), StateChangeComparer());

  for (std::vector<StateChange *>::iterator i = Changes.begin();
       i != Changes.end();
       i++) {
    if (dynamic_cast<ObjectCreate *>(*i) != NULL) {
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
    if (dynamic_cast<ObjectCreate *>(*i) == NULL) {
      if (DebugMode)
	(*i)->Identify();
      (*i)->Report();
      (*i)->Perform();
    }
  }

  Changes.clear();
}

void FileState::ReportChanges()
{
  std::stable_sort(Changes.begin(), Changes.end(), StateChangeComparer());

  for (std::vector<StateChange *>::iterator i = Changes.begin();
       i != Changes.end();
       i++)
    (*i)->Report();
}

void FileState::WriteTo(const std::string& path)
{
  FileInfo info(path);

  info.CreateDirectory();
  if (info.Exists())
    info.Delete();

  std::ofstream fout(path.c_str());
  Root->WriteTo(fout, true);
}

StateEntry * FileState::ReadEntry(StateEntry *		    parent,
				  const std::string&	    path,
				  const std::string&	    relativePath,
				  const std::list<Regex *>& IgnoreList,
				  bool			    verboseOutput)
{
  for (std::list<Regex *>::const_iterator i = IgnoreList.begin();
       i != IgnoreList.end();
       i++)
    if ((*i)->IsMatch(path))
      return NULL;

  StateEntry * entry =
    new StateEntry(this, parent, FileInfo(path, relativePath));

  if (entry->Info.FileKind() == FileInfo::Directory)
    ReadDirectory(entry, path, relativePath, IgnoreList, verboseOutput);

  return entry;
}

void FileState::ReadDirectory(StateEntry *		entry,
			      const std::string&	path,
			      const std::string&	relativePath,
			      const std::list<Regex *>& IgnoreList,
			      bool			verboseOutput)
{
  if (verboseOutput && Path::Parts(relativePath) < 4)
    std::cout << "Scanning: " << entry->Info.FullName << std::endl;

  try {
    std::list<FileInfo> infos;
    entry->Info.GetFileInfos(infos);
    for (std::list<FileInfo>::iterator i = infos.begin();
	 i != infos.end();
	 i++)
      ReadEntry(entry, (*i).FullName, (*i).RelativeName,
		IgnoreList, verboseOutput);
  }
  catch (...) {
    // Defend against access violations and failures to read!
  }
}

FileState * FileState::CreateDatabase(const std::string& path,
				      const std::string& dbfile,
				      bool		 verbose)
{
  std::list<Regex *> ignoreList;
  ignoreList.push_back(new Regex("\\.files\\.db"));

  std::list<std::string> collection;
  collection.push_back(path);

  FileState * tree = FileState::ReadState(collection, ignoreList, verbose);
  if (! dbfile.empty())
    tree->WriteTo(dbfile);

  if (DebugMode) {
    std::cout << "created database state:";
    tree->Report();
  }
  return tree;
}

FileState * FileState::Referent(bool verbose)
{
  std::list<std::string> collection;

  for (StateEntry::children_map::iterator i = Root->Children.begin();
       i != Root->Children.end();
       i++)
    collection.push_back((*i).second->Info.FullName);

  std::list<Regex *> ignoreList;
  ignoreList.push_back(new Regex("\\.files\\.db"));

  FileState * tree = FileState::ReadState(collection, ignoreList, verbose);
  if (DebugMode) {
    std::cout << "read database referent:";
    tree->Report();
  }
  return tree;
}

} // namespace Attic
