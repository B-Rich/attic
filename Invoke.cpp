/*
 *  Invoke.cpp
 *  Attic
 *
 *  Created by John Wiegley on 11/19/06.
 *  Copyright 2006 __MyCompanyName__. All rights reserved.
 *
 */

#include "Invoke.h"

#include "Manager.h"
#include "Posix.h"
#include "FlatDB.h"

void InvokeSychronization(Attic::MessageLog& log)
{
  using namespace Attic;

  Manager atticManager(log);

  DataPool * pool = atticManager.CreatePool();
  pool->LoggingOnly = true;
  pool->SetAncestor(new FlatDatabaseBroker("/home/johnw/src/attic/files.db"));
  pool->AddLocation(new PosixVolumeBroker("/home/johnw/src/misc"));
  pool->AddLocation(new PosixVolumeBroker("/tmp/misc"));

  for (std::vector<Location *>::iterator i = pool->Locations.begin();
       i != pool->Locations.end();
       i++)
    if (*i != pool->CommonAncestor &&
	*i != pool->Locations.back())
      (*i)->PreserveChanges = true;

  atticManager.Synchronize();
}
