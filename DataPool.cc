#include "DataPool.h"

namespace Attic {

DataPool::~DataPool()
{
  if (CommonAncestor)
    delete CommonAncestor;
  if (AllChanges)
    delete AllChanges;
    
  for (std::vector<Location *>::iterator i = Locations.begin();
       i != Locations.end();
       i++)
    delete *i;
}

void DataPool::ComputeChanges()
{
  if (AllChanges)
    delete AllChanges;
  AllChanges = new StateChangesMap;

  for (std::vector<Location *>::iterator i = Locations.begin();
       i != Locations.end();
       i++)
    if ((*i)->PreserveChanges)
      (*i)->ComputeChanges(CommonAncestor, *AllChanges);
}

void DataPool::ResolveConflicts()
{
  for (StateChangesMap::iterator j = AllChanges->begin();
       j != AllChanges->end();
       j++)
    if ((*j).second->Next)
      std::cout << "There are conflicts!" << std::endl;
}

void DataPool::ApplyChanges(MessageLog& log)
{
  if (! AllChanges)
    return;

  StateChangesArray changesArray;

  for (StateChangesMap::iterator i = AllChanges->begin();
       i != AllChanges->end();
       i++)
    changesArray.push_back((*i).second);

  std::stable_sort(changesArray.begin(), changesArray.end(),
		   StateChangeComparer());

  for (std::vector<Location *>::iterator i = Locations.begin();
       i != Locations.end();
       i++) {
    StateChangesArray thisChangesArray;

    for (StateChangesArray::iterator j = changesArray.begin();
	 j != changesArray.end();
	 j++) {
      // Ignore changes in the origin's own repository (since the
      // change is already extant there).
      if (*i == (*j)->Item->Repository)
	continue;

      if ((*j)->ChangeKind == StateChange::Add) {
	(*j)->Duplicates = (*j)->ExistsAtLocation(CommonAncestor, *i);
	if ((*j)->Duplicates) {
	  thisChangesArray.push_front(*j);
	  continue;
	}
      }
      thisChangesArray.push_back(*j);
    }
	
    for (StateChangesArray::iterator j = thisChangesArray.begin();
	 j != thisChangesArray.end();
	 j++)
      for (StateChange * ptr = *j; ptr; ptr = ptr->Next)
	if (LoggingOnly) {
	  ptr->Report(log);
	} else {
	  StateChangesMap * changesMap = (*i)->CurrentChanges;
	  if (! changesMap && ! (*i)->PreserveChanges)
	    changesMap = AllChanges;
	  ptr->Execute(log, *i, changesMap);
	}
  }

  // Reflect all of the changes in the ancestor map
  if (! LoggingOnly && CommonAncestor) {
    if (! CommonAncestor->Root)
      CommonAncestor->Root = new FileInfo;

    for (StateChangesArray::iterator j = changesArray.begin();
	 j != changesArray.end();
	 j++)
      (*j)->Execute(CommonAncestor);

    std::ofstream fout(CommonAncestorPath.c_str());
    CommonAncestor->SaveTo(fout);
    fout.close();
  }      
}

} // namespace Attic
