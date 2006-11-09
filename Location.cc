#include "Location.h"
#include "StateChange.h"

namespace Attic {

Location::Location(const std::string& path)
  : SiteBroker(NULL), CurrentState(NULL), CurrentChanges(NULL),
    VolumeSize(0), VolumeQuota(0),
    MaximumSize(0), MaximumPercent(0),
    ArchivalStore(NULL),

    LowBandwidth(false),
    PreserveChanges(false),
    CaseSensitive(true),
    TrustLengthOnly(false),
    TrustTimestamps(true),
    OverwriteCopy(false),
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
  if (! path.empty()) {
    RootPath	= path;
    CurrentPath = path;
    Moniker	= CurrentPath;
  }
}

Location::Location(const std::string& path, const Location& optionTemplate)
  : SiteBroker(NULL), CurrentState(NULL), CurrentChanges(NULL),
    VolumeSize(0), VolumeQuota(0), MaximumSize(0), MaximumPercent(0),
    ArchivalStore(NULL)
{
  RootPath    = path;
  CurrentPath = path;
  Moniker     = CurrentPath;

  CopyOptions(optionTemplate);
}

Location::~Location()
{
  if (SiteBroker)     delete SiteBroker;
  if (CurrentState)   delete CurrentState;
  if (CurrentChanges) delete CurrentChanges;

  for (std::vector<Regex *>::iterator i = Regexps.begin();
       i != Regexps.end();
       i++)
    delete *i;

  if (ArchivalStore)  delete ArchivalStore;
}

void Location::ComputeChanges(const StateMap * ancestor,
			      StateChangesMap& changesMap)
{
  if (CurrentChanges)
    delete CurrentChanges;
  CurrentChanges = new StateChangesMap;

  if (! CurrentState)
    CurrentState = new StateMap(new FileInfo("", NULL, this));

  CurrentState->CompareTo(ancestor, *CurrentChanges);

  for (StateChangesMap::iterator i = CurrentChanges->begin();
       i != CurrentChanges->end();
       i++) {
    for (StateChange * ptr = (*i).second; ptr; ptr = ptr->Next)
      PostChange(changesMap, ptr->ChangeKind, ptr->Item, ptr->Ancestor);
  }
}

void Location::ApplyChanges(const StateChangesMap& changes)
{
  // jww (2006-11-07): implement
}

void Location::CopyOptions(const Location& optionTemplate)
{
  LowBandwidth	      = optionTemplate.LowBandwidth;
  PreserveChanges     = optionTemplate.PreserveChanges;
  CaseSensitive	      = optionTemplate.CaseSensitive;
  TrustLengthOnly     = optionTemplate.TrustLengthOnly;
  TrustTimestamps     = optionTemplate.TrustTimestamps;
  OverwriteCopy	      = optionTemplate.OverwriteCopy;
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

} // namespace Attic
