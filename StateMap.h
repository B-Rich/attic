#ifndef _FILESTATE_H
#define _FILESTATE_H

#include <vector>

#include "Regex.h"
#include "StateEntry.h"
#include "StateChange.h"

namespace Attic {

class StateMap
{
public:
  StateEntry * Root;

  std::vector<StateChange *> Changes;

  StateMap() : Root(NULL) {}

  StateMap(const std::string& rootPath)
    : Root(new StateEntry(this, NULL, rootPath)) {}

  ~StateMap() {
    delete Root;
  }

  void BindToFile(const std::string& dbPath) {
  }

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

  static StateMap * CreateDatabase(const std::string& path,
				    const std::string& dbfile,
				    bool verbose);

  StateMap * Referent(bool verbose);

  static StateMap * ReadState(const std::deque<std::string>& paths,
			       const std::deque<Regex *>& IgnoreList,
			       bool verboseOutput);
    
  StateEntry * ReadEntry(StateEntry * parent, const std::string& path,
			 const std::string& relativePath,
			 const std::deque<Regex *>& IgnoreList,
			 bool verboseOutput);

  void ReadDirectory(StateEntry * entry, const std::string& path,
		     const std::string& relativePath,
		     const std::deque<Regex *>& IgnoreList,
		     bool verboseOutput);

  static StateMap * ReadState(const std::string& path,
			       const std::deque<Regex *>& IgnoreList,
			       bool verboseOutput);

  StateEntry * LoadElements(char *& data, StateEntry * parent) {
    return StateEntry::LoadElement(this, parent, "", "", data);
  }

  void BackupEntry(StateEntry * entry);
  void MergeChanges(StateMap * other);
  void PerformChanges();
  void ReportChanges();

  void CompareTo(StateMap * other, StateMap * target = NULL) {
    Root->CompareTo(other->Root, target ? target->Root : NULL);
  }

  void Report() const {
    Root->Report();
  }

  void WriteTo(const std::string& path);
};

} // namespace Attic

#endif // _FILESTATE_H
