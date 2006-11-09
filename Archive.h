#ifndef _ARCHIVE_H
#define _ARCHIVE_H

#include <string>

#include <sys/stat.h>

namespace Attic {

// An Archive is a location -- always related to another location --
// where generational backups are kept whenever a destructive update
// is performed.

class Location;
class Archive
{
public:
  Location *  Storage;
  int	      Generations;
  std::string DateFormat;
  mode_t      DefaultFileMode;
};

} // namespace Attic

#endif // _ARCHIVE_H
