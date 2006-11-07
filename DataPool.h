#ifndef _DATAPOOL_H
#define _DATAPOOL_H

#include "error.h"
#include "Location.h"

#include <vector>
#include <string>

namespace Attic {

class Manager
{
};

class Job
{
};

class Closure
{
};

template <typename T>
class TypedClosure
{
};

// A DataPool represents a collection of data which is to be
// replicated amongst two or more target DirectoryTree's.

class Location;

class DataPool
{
public:
  std::vector<Location *> Locations;

  // This state map is the common ancestor of all the Locations listed
  // below.  If there is no CommonAncestor, then reconciliation takes
  // place by causing all locations after the first to simply mimic
  // the first -- meaning that bidirectional updating cannot take
  // place.
  StateMap * CommonAncestor;

  DataPool() : CommonAncestor(NULL) {}
  ~DataPool();

  void BindAncestorToFile(const std::string& path) {
    CommonAncestor = new StateMap;
    CommonAncestor->BindToFile(path);
  }
  void BindAncestorToPath(const std::string& path) {
    CommonAncestor = new StateMap(path);
  }
};

class Broker
{
public:
  void GetSignature();
  void ApplyDelta();
  void ReceiveFile();
  void TransmitFile();
};

class Socket;

class RemoteBroker
{
public:
  std::string  Hostname;
  unsigned int PortNumber;
  std::string  Protocol;
  std::string  Username;
  std::string  Password;

private:
  Socket *     Connection;
};

// An Archive is a location -- always related to another location --
// where generational backups are kept whenever a destructive update
// is performed.

class Archive
{
public:
  Location *  Storage;
  int	      Generations;
  std::string DateFormat;
  mode_t      DefaultFileMode;
};

} // namespace Attic

#endif // _DATAPOOL_H
