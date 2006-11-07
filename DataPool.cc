#include "DataPool.h"

namespace Attic {

DataPool::~DataPool()
{
  if (CommonAncestor)
    delete CommonAncestor;
    
  for (std::vector<Location *>::iterator i = Locations.begin();
       i != Locations.end();
       i++)
    delete *i;
}

} // namespace Attic
