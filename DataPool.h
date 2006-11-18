#ifndef _DATAPOOL_H
#define _DATAPOOL_H

#include "Location.h"
#include "MessageLog.h"

#include <vector>
#include <string>

namespace Attic {

// A DataPool represents a collection of data which is to be
// replicated amongst two or more target DirectoryTree's.

class DataPool
{
public:
  Location * CommonAncestor;
  std::vector<Location *> Locations;

  // This state map is the common ancestor of all the Locations listed
  // below.  If there is no CommonAncestor, then reconciliation takes
  // place by causing all locations after the first to simply mimic
  // the first -- meaning that bidirectional updating cannot take place.
  ChangeSet * AllChanges;

  bool LoggingOnly;

  DataPool() : CommonAncestor(NULL), AllChanges(NULL), LoggingOnly(false) {}
  ~DataPool();

  void Initialize() {
    for (std::vector<Location *>::iterator i = Locations.begin();
	 i != Locations.end();
	 i++)
      (*i)->Initialize();
  }

  void RegisterAncestor(Location * ancestor) {
    CommonAncestor = ancestor;
  }

  void ComputeChanges();
  void ResolveConflicts();
  void ApplyChanges(MessageLog& log);
};

} // namespace Attic

#endif // _DATAPOOL_H
