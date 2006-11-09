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

void DataPool::ApplyChanges(std::ostream& out)
{
  if (! AllChanges)
    return;

  for (std::vector<Location *>::iterator i = Locations.begin();
       i != Locations.end();
       i++)
    for (StateChangesMap::iterator j = AllChanges->begin();
	 j != AllChanges->end();
	 j++)
      if (*i != (*j).second->Item->Repository) {
	if ((*i)->LoggingOnly) {
	  (*j).second->Report(out);
	} else {
	  (*j).second->Execute(out, *i);
	}
      }

  // Reflect all of the changes in the ancestor map
  if (CommonAncestor) {
    if (! CommonAncestor->Root)
      CommonAncestor->Root = new FileInfo;

    for (StateChangesMap::iterator j = AllChanges->begin();
	 j != AllChanges->end();
	 j++)
      (*j).second->Execute(CommonAncestor);

    std::ofstream fout(CommonAncestorPath.c_str());
    CommonAncestor->SaveTo(fout);
    fout.close();
  }      
}

} // namespace Attic
