#include "StateChange.h"
#include "FileState.h"
#include "StateEntry.h"

#include <iostream>

namespace Attic {

int ObjectCreate::Depth() const
{
  return dirToCreate->Depth;
}

void ObjectCreate::Identify()
{
  std::cout << "ObjectCreate(" << DatabaseOnly << ", "
	    << dirToCreate << " " << dirToCreate->Info.FullName << ", "
	    << dirToImitate << " " << dirToImitate->Info.FullName << ")"
	    << std::endl;
}

void ObjectCreate::Perform()
{
  if (DatabaseOnly) return;
  Directory::CreateDirectory(dirToCreate->Info.FullName);
  dirToImitate->Info.CopyAttributes(dirToCreate->Info);
}

void ObjectCreate::Report()
{
  if (DatabaseOnly) return;
  std::cout << dirToCreate->Info.FullName << ": created" << std::endl;
}

int ObjectCopy::Depth() const
{
  return dirToCopyTo->Depth + 1;
}

void ObjectCopy::Identify()
{
  std::cout << "ObjectCopy(" << DatabaseOnly << ", "
	    << entryToCopyFrom << " " << entryToCopyFrom->Info.FullName << ", "
	    << dirToCopyTo << " " << dirToCopyTo->Info.FullName << ")"
	    << std::endl;
}

void ObjectCopy::Prepare()
{
  if (DatabaseOnly) return;
  if (entryToCopyFrom->Copy(dirToCopyTo, true)) {
    copyDone = true;
    std::cout << Path::Combine(dirToCopyTo->Info.FullName,
			       entryToCopyFrom->Info.Name)
	      << ": copied (by moving)" << std::endl;
  }
}

void ObjectCopy::Perform()
{
  if (copyDone) return;
  if (DatabaseOnly) {
    dirToCopyTo->FindOrCreateChild(entryToCopyFrom->Info.Name);
    dirToCopyTo->Info.Reset();
  } else {
    entryToCopyFrom->Copy(dirToCopyTo, false);
  }
}

void ObjectCopy::Report()
{
  if (DatabaseOnly) return;
  if (! copyDone)
    std::cout << Path::Combine(dirToCopyTo->Info.FullName,
			       entryToCopyFrom->Info.Name)
	      << ": copied" << std::endl;
}

int ObjectUpdate::Depth() const
{
  return entryToUpdate->Depth + 1;
}

void ObjectUpdate::Identify()
{
  std::cout << "ObjectUpdate(" << DatabaseOnly << ", "
	    << entryToUpdate << " " << entryToUpdate->Info.FullName << ", "
	    << entryToUpdateFrom << " " << entryToUpdateFrom->Info.FullName << ", \""
	    << reason << "\")"
	    << std::endl;
}

void ObjectUpdate::Prepare()
{
  if (DatabaseOnly) return;
  entryToUpdate->FileStateObj->BackupEntry(entryToUpdate);
}

void ObjectUpdate::Perform()
{
  if (! DatabaseOnly) {
    assert(entryToUpdateFrom->Info.FileKind() == entryToUpdate->Info.FileKind());
    assert(entryToUpdateFrom->Info.FileKind() == FileInfo::File);
    entryToUpdateFrom->Copy(entryToUpdate->Info.FullName);
  }

  // Force the entry to update itself when the database is written
  entryToUpdate->Info.Reset();
}

void ObjectUpdate::Report()
{
  if (DatabaseOnly) return;
  if (! reason.empty())
    std::cout << entryToUpdate->Info.FullName << ": updated" << std::endl;
}

int ObjectUpdateProps::Depth() const
{
  return entryToUpdate->Depth + 1;
}

void ObjectUpdateProps::Identify()
{
  std::cout << "ObjectUpdateProps(" << DatabaseOnly << ", "
	    << entryToUpdate << " " << entryToUpdate->Info.FullName << ", "
	    << entryToUpdateFrom << " " << entryToUpdateFrom->Info.FullName << ", \""
	    << reason << "\")"
	    << std::endl;
}

void ObjectUpdateProps::Perform()
{
  entryToUpdateFrom->Info.CopyAttributes(entryToUpdate->Info, DatabaseOnly);
}

void ObjectUpdateProps::Report()
{
  if (DatabaseOnly) return;
  if (! reason.empty())
    std::cout << entryToUpdate->Info.FullName << ": props updated" << std::endl;
}

int ObjectDelete::Depth() const
{
  return entryToDelete->Depth + 1;
}

void ObjectDelete::Identify()
{
  std::cout << "ObjectDelete(" << DatabaseOnly << ", "
	    << entryToDelete << " " << entryToDelete->Info.FullName << ")"
	    << std::endl;
}

void ObjectDelete::Prepare()
{
  if (DatabaseOnly) return;
  entryToDelete->MarkDeletePending();
  entryToDelete->FileStateObj->BackupEntry(entryToDelete);
}

void ObjectDelete::Perform()
{
  if (DatabaseOnly || entryToDelete->DeletePending) {
    StateEntry::children_map::iterator i =
      entryToDelete->Parent->Children.find(entryToDelete->Info.Name);
    assert(i != entryToDelete->Parent->Children.end());
    entryToDelete->Parent->Children.erase(i);
  }

  if (entryToDelete->DeletePending) {
    if (entryToDelete->Exists())
      entryToDelete->Delete();
    entryToDelete->DeletePending = false;
  }
}

void ObjectDelete::Report()
{
  if (DatabaseOnly) return;
  std::cout << entryToDelete->Info.FullName << ": deleted" << std::endl;
}

} // namespace Attic
