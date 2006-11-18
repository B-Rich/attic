#ifndef _LOCATION_H
#define _LOCATION_H

#include "Regex.h"
#include "Broker.h"
#include "Archive.h"
#include "ChangeSet.h"

#include <map>
#include <vector>
#include <iostream>
#include <fstream>
#include <ctime>

namespace Attic {

// A Location represents a directory on a mounted volume or a remote
// host, with an associated state map.

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

  mutable FileInfo * RootEntry;

  FileInfo * Root() const {
    return NULL;
  }

  typedef std::map<md5sum_t, FileInfoArray>  ChecksumMap;
  typedef std::pair<md5sum_t, FileInfoArray> ChecksumPair;

  ChecksumMap EntriesByChecksum;

  void RegisterChecksums(FileInfo * entry);
  FileInfoArray * FindDuplicates(const FileInfo * item);

  // If a location supports bidirectional updating, it will be
  // determined upon connection how it now differs from the DataPool's
  // CommonAncestor.  If these changes can be determined, after the
  // reconciliation phase there will be a CurrentChanges map
  // reflecting all the changes that would have happen to the common
  // ancestor tree to make it identical to this Location.  Thus, if
  // the only change at this location were the deletion of a file, the
  // CurrentChanges map would point to a single DeleteChange object
  // representing this change in state.
  mutable ChangeSet * CurrentChanges;

  void Sync() const { SiteBroker->Sync(); }
  void ApplyChanges(const ChangeSet& changeSet);

  std::vector<Regex *> Regexps;

  // If LowBandwidth is true, signature files will be kept in the
  // state map for the common ancestor so that this data need not be
  // transferred to us before we begin sending deltas.  Note that this
  // can get expensive in terms of disk space on the volume that
  // stores the ancestor's state map!
  bool LowBandwidth;		// -L if true, optimize traffic at all costs
  bool PreserveChanges;		// -b if true, location changes are preserved
  bool CaseSensitive;		// -c if true, file system is case sensitive
  bool TrustLengthOnly;		//    if true, only check files based on size
  bool TrustTimestamps;		//    if false, checksum to detect changes (-c)
  bool CopyByOverwrite;		// -O if true, copy directly over the target
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
  bool ExcludeCVS;		// -C if true, exclude files related to CVS

  // Initialize this location using optionTemplate to determine the
  // default values for options.
  Location(Broker * _SiteBroker = NULL);
  Location(Broker * _SiteBroker, const Location& optionTemplate);
  ~Location();

  void CopyOptions(const Location& optionTemplate);
  void Initialize();

  FileInfoArray * ExistsAtLocation(const FileInfo * Item) const {
#if 0
    Location * currentState = SiteBroker;
    if (! currentState)
      currentState = StateBroker;
    return currentState->FindDuplicates(Item);
#else
    return NULL;
#endif
  }

  void ApplyChange(MessageLog * log, const StateChange& change,
		   const ChangeSet& changeSet);
};

} // namespace Attic

#endif // _LOCATION_H
