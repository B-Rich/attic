#ifndef _FILESTATE_H
#define _FILESTATE_H

#include <vector>

#include "Regex.h"
#include "StateChange.h"
#include "StateEntry.h"

namespace Attic {

class FileState
{
public:
  StateEntry * Root;
  FileInfo *   GenerationPath;

  std::vector<StateChange *> Changes;

  void RegisterUpdate(StateEntry * baseEntryToUpdate,
		      StateEntry * entryToUpdate,
		      StateEntry * entryToUpdateFrom,
		      const std::string& reason = "");

  void RegisterUpdateProps(StateEntry * baseEntryToUpdate,
			   StateEntry * entryToUpdate,
			   StateEntry * entryToUpdateFrom,
			   const std::string& reason = "");

  void RegisterDelete(StateEntry * baseEntry, StateEntry * entry);
  void RegisterCopy(StateEntry * entry, StateEntry * baseDirToCopyTo,
		    StateEntry * dirToCopyTo);

  typedef std::map<md5sum_t, StateEntry *>  checksum_map;
  typedef std::pair<md5sum_t, StateEntry *> checksum_pair;

  checksum_map ChecksumDict;

  StateEntry * FindDuplicate(StateEntry * child);

  static FileState * CreateDatabase(const std::string& path,
				    const std::string& dbfile,
				    bool verbose);

  FileState * Referent(bool verbose);

  static FileState * ReadState(const std::list<std::string>& paths,
			       const std::list<Regex *>& IgnoreList,
			       bool verboseOutput);
    
  StateEntry * ReadEntry(StateEntry * parent, const std::string& path,
			 const std::string& relativePath,
			 const std::list<Regex *>& IgnoreList,
			 bool verboseOutput);

  void ReadDirectory(StateEntry * entry, const std::string& path,
		     const std::string& relativePath,
		     const std::list<Regex *>& IgnoreList,
		     bool verboseOutput);

  static FileState * ReadState(const std::string& path,
			       const std::list<Regex *>& IgnoreList,
			       bool verboseOutput);

  StateEntry * LoadElements(char *& data, StateEntry * parent) {
    return StateEntry::LoadElement(this, parent, "", "", data);
  }

  void BackupEntry(StateEntry * entry);
  void MergeChanges(FileState * other);
  void PerformChanges();
  void ReportChanges();

  void CompareTo(FileState * other, FileState * target = NULL) {
    Root->CompareTo(other->Root, target ? target->Root : NULL);
  }

  void Report() const {
    Root->Report();
  }

  void WriteTo(const std::string& path);
};

} // namespace Attic

#endif // _FILESTATE_H
