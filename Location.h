#ifndef _LOCATION_H
#define _LOCATION_H

#include "Regex.h"
#include "FileInfo.h"
#include "StateChange.h"
#include "StateMap.h"
#include "StateEntry.h"

#include <map>
#include <iostream>
#include <fstream>
#include <ctime>

namespace Attic {

// A Location represents a directory on a mounted volume or a remote
// host, with an associated state map.

typedef std::map<std::string, StateChange *>  StateChangesMap;
typedef std::pair<std::string, StateChange *> StateChangesPair;

class Archive;
class Broker;

class Location
{
public:
  // A Location's Broker is responsible for communicating state
  // details to us (when applicable) and for accepting and
  // transmitting files for updating either side.  For local
  // transfers, the broker is resident within the Manager process; for
  // network transfers, it is a remote invocation of the program in
  // slave mode on the Location's host.
  Broker * SiteBroker;

  // If a location supports bidirectional updating, its CurrentState
  // will be determined upon connection to detect how it now differs
  // from the DataPool's CommonAncestor.  If a CurrentState can be
  // determined, then after the reconciliation phase there will be a
  // CurrentChanges map reflecting all the changes that would have
  // happen to the common ancestor tree to make it identical to this
  // Location.  Thus, if the only change at this location were the
  // deletion of a file, the CurrentChanges map would point to a
  // single DeleteChange object representing this change in state.
  StateMap	  CurrentState;
  StateChangesMap CurrentChanges;

  FileInfo& RootPath() {
    return CurrentState.Root->Info;
  }

  // If LowBandwidth is true, signature files will be kept in the
  // state map for the common ancestor so that this data need not be
  // transferred to us before we begin sending deltas.  Note that this
  // can get expensive in terms of disk space on the volume that
  // stores the ancestor's state map!
  bool LowBandwidth;		// -L if true, optimize traffic at all costs
  bool Bidirectional;		// -b if true, location changes are preserved
  bool CaseSensitive;		// -c if true, file system is case sensitive
  bool TrustLengthOnly;		//    if true, only check files based on size
  bool TrustTimestamps;		//    if false, checksum to detect changes (-c)
  bool OverwriteCopy;		// -O if true, copy directly over the target
  bool CopyWholeFiles;		//    if true, do not use the rsync algorithm
  bool ChecksumVerify;		// -V if true, do a checksum after every copy
  bool VerifyResults;		//    if true, verify location's state after
  bool SecureDelete;		// -S if true, securely wipe any deleted file
  bool DeleteBeforeUpdate;	// -D if true, delete files before changing them
  bool PreserveTimestamps;	// -t if true, timestamps are part of state
  bool PreserveHardLinks;	// -H if true, hard links are part of state
  bool PreserveSoftLinks;	// -l if true, do not follow symbolic links -L
  bool PreserveOwnership;	// -o if true, ownership details are part of state
  bool PreserveGroup;		// -g if true, ownership details are part of state
  bool PreservePermissions;	// -p if true, file permissions are part of state
  bool PreserveSparseFiles;	// -S if true, "sparse"-ness is a part of state
  bool EncodeFilenames;		// -@ if true, use lookup table for long filenames
  bool ExtendedAttributes;	// -E if true, filesystem uses resource forks
  bool CompressFiles;		//    if true, compress all files at location
  bool CompressTraffic;		// -z if true, compress all files in transit
  bool EncryptFiles;		//    if true, encrypt all files at location
  bool EncryptTraffic;		//    if true, encrypt all files in transit
  bool RespectBoundaries;	// -x if true, never cross filesystem boundaries
  bool LoggingOnly;		// -n if true, don't transfer, just log operations
  bool VerboseLogging;		// -v if true, make logging much more verbose

  // RCS SCCS CVS  CVS.adm  RCSLOG  cvslog.*  tags  TAGS  .make.state
  // .nse_depinfo  *~ #* .#* ,* _$* *$ *.old *.bak *.BAK *.orig *.rej
  // .del-* *.a *.olb *.o *.obj *.so *.exe *.Z *.elc *.ln core .svn/
  bool ExcludeCVS;
  std::vector<Regex *> Regexps;

  std::string  VolumePath;
  unsigned int VolumeSize;
  unsigned int VolumeQuota;
  unsigned int MaximumSize;
  unsigned int MaximumPercent;
  Archive *    ArchivalStore;
  std::string  TempDirectory;
  std::string  CompressionScheme;
  std::string  CompressionOptions;
  std::string  EncryptionScheme;
  std::string  EncryptionOptions;

  // Initialize this location using optionTemplate to determine the
  // default values for options.
  Location(const std::string& path = "")
    : SiteBroker(NULL),
      CurrentState(path),

      LowBandwidth(false),
      Bidirectional(false),
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

      ExcludeCVS(false),

      VolumeSize(0),
      VolumeQuota(0),
      MaximumSize(0),
      MaximumPercent(0),

      ArchivalStore(NULL) {}

  void CopyOptions(const Location& optionTemplate)
  {
      LowBandwidth	  = optionTemplate.LowBandwidth;
      Bidirectional	  = optionTemplate.Bidirectional;
      CaseSensitive	  = optionTemplate.CaseSensitive;
      TrustLengthOnly	  = optionTemplate.TrustLengthOnly;
      TrustTimestamps	  = optionTemplate.TrustTimestamps;
      OverwriteCopy	  = optionTemplate.OverwriteCopy;
      CopyWholeFiles	  = optionTemplate.CopyWholeFiles;
      ChecksumVerify	  = optionTemplate.ChecksumVerify;
      VerifyResults	  = optionTemplate.VerifyResults;
      SecureDelete	  = optionTemplate.SecureDelete;
      DeleteBeforeUpdate  = optionTemplate.DeleteBeforeUpdate;
      PreserveTimestamps  = optionTemplate.PreserveTimestamps;
      PreserveHardLinks	  = optionTemplate.PreserveHardLinks;
      PreserveSoftLinks	  = optionTemplate.PreserveSoftLinks;
      PreserveOwnership	  = optionTemplate.PreserveOwnership;
      PreserveGroup	  = optionTemplate.PreserveGroup;
      PreservePermissions = optionTemplate.PreservePermissions;
      PreserveSparseFiles = optionTemplate.PreserveSparseFiles;
      EncodeFilenames	  = optionTemplate.EncodeFilenames;
      ExtendedAttributes  = optionTemplate.ExtendedAttributes;
      CompressFiles	  = optionTemplate.CompressFiles;
      CompressTraffic	  = optionTemplate.CompressTraffic;
      EncryptFiles	  = optionTemplate.EncryptFiles;
      EncryptTraffic	  = optionTemplate.EncryptTraffic;
      RespectBoundaries	  = optionTemplate.RespectBoundaries;
      LoggingOnly	  = optionTemplate.LoggingOnly;
      VerboseLogging	  = optionTemplate.VerboseLogging;
      ExcludeCVS	  = optionTemplate.ExcludeCVS;

      Regexps.clear();

      for (std::vector<Regex *>::const_iterator
	     i = optionTemplate.Regexps.begin();
	   i != optionTemplate.Regexps.end();
	   i++)
	Regexps.push_back(new Regex(**i));
  }
};

} // namespace Attic

#endif // _LOCATION_H
