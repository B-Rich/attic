#include "Location.h"
#include "StateChange.h"

namespace Attic {

Location::Location(Broker * _SiteBroker)
  : SiteBroker(_SiteBroker),
    CurrentChanges(NULL),

    LowBandwidth(false),
    PreserveChanges(false),
    CaseSensitive(true),
    TrustLengthOnly(false),
    TrustTimestamps(true),
    CopyByOverwrite(false),
    CopyWholeFiles(false),
    ChecksumVerify(false),
    VerifyResults(false),
    SecureDelete(false),
    DeleteBeforeUpdate(false),
    PreserveTimestamps(true),
    PreserveHardLinks(true),
    PreserveSoftLinks(true),
    PreserveOwnership(true),
    PreserveGroup(true),
    PreservePermissions(true),
    PreserveSparseFiles(true),
    EncodeFilenames(false),
    ExtendedAttributes(true),
    CompressFiles(false),
    CompressTraffic(false),
    EncryptFiles(false),
    EncryptTraffic(false),
    RespectBoundaries(false),
    LoggingOnly(false),
    VerboseLogging(false),
    ExcludeCVS(false)
{
#if 0
  if (SiteBroker)
    SiteBroker->SetRepository(this);
#endif
}

Location::Location(Broker * _SiteBroker, const Location& optionTemplate)
  : SiteBroker(_SiteBroker)
{
#if 0
  if (SiteBroker)
    SiteBroker->SetRepository(this);
#endif
  CopyOptions(optionTemplate);
}

Location::~Location()
{
  if (SiteBroker)
    delete SiteBroker;

  for (std::vector<Regex *>::iterator i = Regexps.begin();
       i != Regexps.end();
       i++)
    delete *i;
}

#if 0
void Location::ComputeChanges(const Location * ancestor, ChangeSet& changeSet)
{
  if (CurrentChanges)
    delete CurrentChanges;
  CurrentChanges = new ChangeSet;

  if (! CurrentState)
    CurrentState = new StateMap(new FileInfo("", NULL, this));

  CurrentChanges->CompareLocations(this, ancestor);

  for (ChangeSet::ChangesMap::iterator i = CurrentChanges->Changes.begin();
       i != CurrentChanges->Changes.end();
       i++) {
    for (StateChange * ptr = (*i).second; ptr; ptr = ptr->Next)
      changeSet.PostChange(ptr->ChangeKind, ptr->Item, ptr->Ancestor);
  }
}
#endif

void Location::CopyOptions(const Location& optionTemplate)
{
  LowBandwidth	      = optionTemplate.LowBandwidth;
  PreserveChanges     = optionTemplate.PreserveChanges;
  CaseSensitive	      = optionTemplate.CaseSensitive;
  TrustLengthOnly     = optionTemplate.TrustLengthOnly;
  TrustTimestamps     = optionTemplate.TrustTimestamps;
  CopyByOverwrite     = optionTemplate.CopyByOverwrite;
  CopyWholeFiles      = optionTemplate.CopyWholeFiles;
  ChecksumVerify      = optionTemplate.ChecksumVerify;
  VerifyResults	      = optionTemplate.VerifyResults;
  SecureDelete	      = optionTemplate.SecureDelete;
  DeleteBeforeUpdate  = optionTemplate.DeleteBeforeUpdate;
  PreserveTimestamps  = optionTemplate.PreserveTimestamps;
  PreserveHardLinks   = optionTemplate.PreserveHardLinks;
  PreserveSoftLinks   = optionTemplate.PreserveSoftLinks;
  PreserveOwnership   = optionTemplate.PreserveOwnership;
  PreserveGroup	      = optionTemplate.PreserveGroup;
  PreservePermissions = optionTemplate.PreservePermissions;
  PreserveSparseFiles = optionTemplate.PreserveSparseFiles;
  EncodeFilenames     = optionTemplate.EncodeFilenames;
  ExtendedAttributes  = optionTemplate.ExtendedAttributes;
  CompressFiles	      = optionTemplate.CompressFiles;
  CompressTraffic     = optionTemplate.CompressTraffic;
  EncryptFiles	      = optionTemplate.EncryptFiles;
  EncryptTraffic      = optionTemplate.EncryptTraffic;
  RespectBoundaries   = optionTemplate.RespectBoundaries;
  LoggingOnly	      = optionTemplate.LoggingOnly;
  VerboseLogging      = optionTemplate.VerboseLogging;
  ExcludeCVS	      = optionTemplate.ExcludeCVS;

  Regexps.clear();

  for (std::vector<Regex *>::const_iterator
	 i = optionTemplate.Regexps.begin();
       i != optionTemplate.Regexps.end();
       i++)
    Regexps.push_back(new Regex(**i));
}

void Location::Initialize()
{
  // RCS SCCS CVS  CVS.adm  RCSLOG  cvslog.*  tags  TAGS  .make.state
  // .nse_depinfo  *~ #* .#* ,* _$* *$ *.old *.bak *.BAK *.orig *.rej
  // .del-* *.a *.olb *.o *.obj *.so *.exe *.Z *.elc *.ln core .svn/

  if (ExcludeCVS) {
    Regexps.push_back(new Regex("RCS", true));
    Regexps.push_back(new Regex("SCCS", true));
    Regexps.push_back(new Regex("CVS", true));
    Regexps.push_back(new Regex("CVS.adm", true));
    Regexps.push_back(new Regex("RCSLOG", true));
    Regexps.push_back(new Regex("cvslog.*", true));
    Regexps.push_back(new Regex("tags", true));
    Regexps.push_back(new Regex("TAGS", true));
    Regexps.push_back(new Regex(".make.state", true));
    Regexps.push_back(new Regex(".nse_depinfo", true));
    Regexps.push_back(new Regex("*~", true));
    Regexps.push_back(new Regex("#*", true));
    Regexps.push_back(new Regex(".#*", true));
    Regexps.push_back(new Regex(",*", true));
    Regexps.push_back(new Regex("_$*", true));
    Regexps.push_back(new Regex("*$", true));
    Regexps.push_back(new Regex("*.old", true));
    Regexps.push_back(new Regex("*.bak", true));
    Regexps.push_back(new Regex("*.BAK", true));
    Regexps.push_back(new Regex("*.orig", true));
    Regexps.push_back(new Regex("*.rej", true));
    Regexps.push_back(new Regex(".del-*", true));
    Regexps.push_back(new Regex("*.a", true));
    Regexps.push_back(new Regex("*.olb", true));
    Regexps.push_back(new Regex("*.o", true));
    Regexps.push_back(new Regex("*.obj", true));
    Regexps.push_back(new Regex("*.so", true));
    Regexps.push_back(new Regex("*.exe", true));
    Regexps.push_back(new Regex("*.Z", true));
    Regexps.push_back(new Regex("*.elc", true));
    Regexps.push_back(new Regex("*.ln", true));
    Regexps.push_back(new Regex("core", true));
    Regexps.push_back(new Regex(".svn/", true));
  }
}

void Location::ApplyChange(MessageLog * log, const StateChange& change,
			   const ChangeSet& changeSet)
{
  Path	   targetPath(change.Item->FullName);
  FileInfo targetInfo(targetPath);

  std::string label;
  switch (change.ChangeKind) {
  case StateChange::Add:
    if (change.Item->IsDirectory()) {
      if (targetInfo.Exists()) {
	if (targetInfo.IsDirectory())
	  return;
	targetInfo.Delete();
	if (log)
	  LOG(*log, Message, "D " << targetPath);
      }
      Directory::CreateDirectory(targetPath);
      label = "c ";
    }
    else if (change.Item->IsRegularFile()) {
      if (targetInfo.Exists()) {
	if (targetInfo.IsRegularFile() &&
	    targetInfo.Checksum() == change.Item->Checksum()) {
	  ChangeSet ignoredChanges;
	  ignoredChanges.CompareFiles(change.Item, &targetInfo);
	  if (! ignoredChanges.Changes.empty()) {
	    change.Item->CopyAttributes(targetPath);
	    label = "p ";
	    break;
	  }
	  return;
	}
	targetInfo.Delete();
	if (log)
	  LOG(*log, Message, "D " << targetPath);
      }

      // If Duplicate is non-NULL (and this applies only for Add
      // changes), then an exact copy of our entry already exists in
      // the target location.  If it is marked for deletion, we'll
      // move it to the target path; if it's not, we'll copy it over
      // there.
      if (change.Duplicates) {
	FileInfo * Duplicate = change.Duplicates->front();

	bool markedForDeletion = false;
	if (CurrentChanges)
	  for (FileInfoArray::const_iterator i = change.Duplicates->begin();
	       i != change.Duplicates->end();
	       i++) {
	    ChangeSet::ChangesMap::const_iterator j =
	      changeSet.Changes.find((*i)->FullName);
	    if (j != changeSet.Changes.end()) {
	      for (StateChange * ptr = (*j).second; ptr; ptr = ptr->Next)
		if (ptr->ChangeKind == StateChange::Remove) {
		  markedForDeletion = true;
		  Duplicate = ptr->Item;
		  break;
		}
	    }
	    if (markedForDeletion)
	      break;
	  }

	if (markedForDeletion) {
	  Directory::CreateDirectory(targetPath.DirectoryName());
	  File::Move(Duplicate->FullName, targetPath);
	  label = "m ";
	  break;
	} else {
	  // jww (2006-11-09): Do this through the broker, since it
	  // must happen remotely
	  Directory::CreateDirectory(targetPath.DirectoryName());
	  File::Copy(Duplicate->FullName, targetPath);
	  label = "u ";
	  break;
	}
      }

      File::Copy(change.Item->Pathname, targetPath);
      label = "U ";
    }
    else {
      assert(0);
    }
    break;

  case StateChange::Remove:
    if (targetInfo.Exists()) {
      if (targetInfo.IsDirectory())
	Directory::Delete(targetPath);
      else
	File::Delete(targetPath);
    }
    label = "D ";
    break;

  case StateChange::Update:
    if (change.Item->IsRegularFile())
      File::Copy(change.Item->Pathname, targetPath);
    else
      assert(0);
    label = "P ";
    break;

  case StateChange::UpdateProps:
    change.Item->CopyAttributes(targetPath);
    label = "p ";
    break;

  default:
    assert(0);
    break;
  }

  if (log)
    LOG(*log, Message, label << SiteBroker->Moniker(change.Item->FullName));
}

void Location::RegisterChecksums(FileInfo * entry)
{
  if (entry->IsRegularFile()) {
    ChecksumMap::iterator i = EntriesByChecksum.find(entry->Checksum());
    if (i == EntriesByChecksum.end()) {
      FileInfoArray entryArray;
      entryArray.push_back(entry);
      EntriesByChecksum.insert(ChecksumPair(entry->Checksum(), entryArray));
    } else {
      (*i).second.push_back(entry);
    }
  }
  else if (entry->IsDirectory()) {
    for (FileInfo::ChildrenMap::const_iterator i = entry->ChildrenBegin();
	 i != entry->ChildrenEnd();
	 i++)
      RegisterChecksums((*i).second);
  }
}

FileInfoArray * Location::FindDuplicates(const FileInfo * item)
{
  if (item->IsRegularFile()) {
    ChecksumMap::iterator i = EntriesByChecksum.find(item->Checksum());
    if (i != EntriesByChecksum.end())
      return &(*i).second;
  }
  return NULL;
}

#if 0
void StateMap::ApplyChange(const StateChange& change)
{
  FileInfo * entry;

  switch (change.ChangeKind) {
  case StateChange::Add:
    entry = Root->FindOrCreateMember(change.Item->FullName);
    change.Item->CopyDetails(*entry, true);
    change.Item->CopyAttributes(*entry, true);
    break;

  case StateChange::Remove:
    entry = Root->FindMember(change.Item->FullName);
    if (entry)
      entry->Parent->DestroyChild(entry);
    break;

  case StateChange::Update:
    entry = Root->FindMember(change.Item->FullName);
    if (entry)
      change.Item->CopyDetails(*entry, true);
    break;

  case StateChange::UpdateProps:
    entry = Root->FindMember(change.Item->FullName);
    if (entry)
      change.Item->CopyAttributes(*entry, true);
    break;

  default:
    assert(0);
    break;
  }
}
#endif

} // namespace Attic
