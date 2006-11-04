#include <list>
#include <vector>
#include <map>
#include <string>
#include <exception>
#include <iostream>
#include <fstream>
#include <ctime>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>
#include <errno.h>
#include <cstdlib>
#define HAVE_GETPWUID
#define HAVE_GETPWNAM
#if defined(HAVE_GETPWUID) || defined(HAVE_GETPWNAM)
#include <pwd.h>
#endif
#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <pcre.h>

#include "binary.h"
#include "md5.h"

#define HAVE_REALPATH
#ifdef HAVE_REALPATH
extern "C" char *realpath(const char *, char resolved_path[]);
#endif

#define BINARY_VERSION 0x00000001L

namespace Attic {

std::string expand_path(const std::string& path)
{
  if (path.length() == 0 || path[0] != '~') {
    char resolved_path[PATH_MAX];
    return realpath(path.c_str(), resolved_path);
  }

  const char * pfx = NULL;
  std::string::size_type pos = path.find_first_of('/');

  if (path.length() == 1 || pos == 1) {
    pfx = std::getenv("HOME");
#ifdef HAVE_GETPWUID
    if (! pfx) {
      // Punt. We're trying to expand ~/, but HOME isn't set
      struct passwd * pw = getpwuid(getuid());
      if (pw)
	pfx = pw->pw_dir;
    }
#endif
  }
#ifdef HAVE_GETPWNAM
  else {
    std::string user(path, 1, pos == std::string::npos ?
		     std::string::npos : pos - 1);
    struct passwd * pw = getpwnam(user.c_str());
    if (pw)
      pfx = pw->pw_dir;
  }
#endif

  // if we failed to find an expansion, return the path unchanged.

  if (! pfx) {
    char resolved_path[PATH_MAX];
    return realpath(path.c_str(), resolved_path);
  }

  if (pos == std::string::npos) {
    char resolved_path[PATH_MAX];
    return realpath(pfx, resolved_path);
  }

  std::string result(pfx);
  if (result.length() == 0 || result[result.length() - 1] != '/')
    result += '/';
  result += path.substr(pos + 1);

  char resolved_path[PATH_MAX];
  return realpath(result.c_str(), resolved_path);
}

class exception : public std::exception
{
protected:
  std::string reason;

public:
  exception(const std::string& _reason) throw()
    : reason(_reason) {}
  virtual ~exception() throw() {}

  virtual const char* what() const throw() {
    return reason.c_str();
  }
};

class Regex
{
  void * regexp;

public:
  bool Exclude;
  std::string Pattern;

  explicit Regex(const std::string& pattern);
  Regex(const Regex&);
  ~Regex();

  bool IsMatch(const std::string& str) const;
};

Regex::Regex(const std::string& pat) : Exclude(false)
{
  const char * p = pat.c_str();
  if (*p == '-') {
    Exclude = true;
    p++;
    while (std::isspace(*p))
      p++;
  }
  else if (*p == '+') {
    p++;
    while (std::isspace(*p))
      p++;
  }
  Pattern = p;

  const char *error;
  int erroffset;
  regexp = pcre_compile(Pattern.c_str(), PCRE_CASELESS,
			&error, &erroffset, NULL);
  if (! regexp)
    throw exception(std::string("Failed to compile regexp '") + Pattern + "'");
}

Regex::Regex(const Regex& m) : Exclude(m.Exclude), Pattern(m.Pattern)
{
  const char *error;
  int erroffset;
  regexp = pcre_compile(Pattern.c_str(), PCRE_CASELESS,
			&error, &erroffset, NULL);
  assert(regexp);
}

Regex::~Regex() {
  if (regexp)
    pcre_free((pcre *)regexp);
}

bool Regex::IsMatch(const std::string& str) const
{
  static int ovec[30];
  int result = pcre_exec((pcre *)regexp, NULL,
			 str.c_str(), str.length(), 0, 0, ovec, 30);
  return result >= 0 && ! Exclude;
}

class Path
{
public:
  static int Parts(const std::string& path)
  {
    int len = path.length();
    int count = 1;
    for (int i = 0; i < len; i++)
      if (path[i] == '/')
	count++;
    return count;
  }

  static std::string GetFileName(const std::string& path)
  {
    int index = path.rfind('/');
    if (index != std::string::npos)
      return path.substr(index + 1);
    else      
      return path;
  }

  static std::string Combine(const std::string& first,
			     const std::string& second)
  {
    return first + "/" + second;
  }
};

class md5sum_t
{
  md5_byte_t digest[16];

public:
  bool operator==(const md5sum_t& other) const {
    return memcmp(digest, other.digest, sizeof(md5_byte_t) * 16) == 0;
  }
  bool operator!=(const md5sum_t& other) const {
    return ! (*this == other);
  }
  bool operator<(const md5sum_t& other) const {
    return memcmp(digest, other.digest, sizeof(md5_byte_t) * 16) < 0;
  }

  static md5sum_t checksum(const std::string& path, md5sum_t& csum) {
    md5_state_t state;
    md5_init(&state);

    char cbuf[8192];

    std::fstream fin(path.c_str());
    fin.read(cbuf, 8192);
    int read = fin.gcount();
    while (read > 0) {
      md5_append(&state, (md5_byte_t *)cbuf, read);
      if (fin.eof() || fin.bad())
	break;
      fin.read(cbuf, 8192);
      read = fin.gcount();
    }

    md5_finish(&state, csum.digest);
  }

  friend std::ostream& operator<<(std::ostream& out, const md5sum_t& md5);
};

std::ostream& operator<<(std::ostream& out, const md5sum_t& md5) {
  for (int i = 0; i < 16; i++) {
    out.fill('0');
    out << std::hex << (int)md5.digest[i];
  }
  return out;
}

class FileInfo
{
  mutable struct stat info;

public:
  mutable bool didstat;

  enum Kind {
    Collection, Directory, File, SymbolicLink, Special, Nonexistant, Last
  };

  std::string Name;
  std::string RelativeName;
  std::string FullName;

  FileInfo() : fileKind(Collection) {
    readcsum = false;
    didstat  = true;
  }

  FileInfo(const std::string& _FullName)
    : FullName(_FullName),
      RelativeName(_FullName),
      Name(Path::GetFileName(_FullName)) {
    Reset();
  }

  FileInfo(const std::string& _FullName, const std::string& _RelativeName)
    : FullName(_FullName),
      RelativeName(_RelativeName),
      Name(Path::GetFileName(_FullName)) {
    Reset();
  }

  mutable Kind fileKind;

  void dostat() const {
    if (lstat(FullName.c_str(), &info) == -1 && errno == ENOENT) {
      fileKind = Nonexistant;
      return;
    }

    didstat = true;

    switch (info.st_mode & S_IFMT) {
    case S_IFDIR:
      fileKind = Directory;
      break;

    case S_IFREG:
      fileKind = File;
      break;

    case S_IFLNK:
      fileKind = SymbolicLink;
      break;

    default:
      fileKind = Special;
      break;
    }
  }

public:
  void Reset() {
    fileKind = Nonexistant;
    didstat  = false;
    readcsum = false;
  }

  Kind FileKind() const {
    if (! didstat) dostat();
    return fileKind;
  }

  bool Exists() const {
    return FileKind() != Nonexistant;
  }

  off_t& Length() {
    if (! didstat) dostat();
    assert(Exists());
    return info.st_size;
  }

  mode_t Permissions() {
    if (! didstat) dostat();
    return info.st_mode & ~S_IFMT;
  }    
  void SetPermissions(mode_t mode) {
    if (! didstat) dostat();
    info.st_mode = (info.st_mode & S_IFMT) | mode;
  }    
  uid_t& OwnerId() {
    if (! didstat) dostat();
    return info.st_uid;
  }    
  gid_t& GroupId() {
    if (! didstat) dostat();
    return info.st_gid;
  }    

  struct timespec& LastWriteTime() {
    if (! didstat) dostat();
    return info.st_mtimespec;
  }
  struct timespec& LastAccessTime() {
    if (! didstat) dostat();
    return info.st_atimespec;
  }

  bool IsDirectory() const {
    return FileKind() == Directory;
  }
  bool IsSymbolicLink() const {
    return FileKind() == SymbolicLink;
  }
  bool IsRegularFile() const {
    return FileKind() == File;
  }

  void CopyAttributes(FileInfo& dest, bool dataOnly = false);
  void CopyAttributes(const std::string& dest);

private:
  mutable md5sum_t csum;
  mutable bool readcsum;

public:
  md5sum_t& Checksum() {
    if (! IsRegularFile())
      throw exception("Attempt to calc checksum of non-file '" + FullName + "'");

    if (! readcsum) {
      md5sum_t::checksum(FullName, csum);
      readcsum = true;
    }
    return csum;
  }

  md5sum_t CurrentChecksum() const {
    if (! IsRegularFile())
      throw exception("Attempt to calc checksum of non-file '" + FullName + "'");

    md5sum_t sum;
    md5sum_t::checksum(FullName, sum);
    return sum;
  }

  FileInfo LinkReference() const {
    if (! IsSymbolicLink())
      throw exception("Attempt to dereference non-symbol link '" + FullName + "'");

    char buf[8192];
    if (readlink(FullName.c_str(), buf, 8191) != -1)
      return FileInfo(buf);
    else
      throw exception("Failed to read symbol link '" + FullName + "'");
  }

  void CreateDirectory();

  void GetFileInfos(std::list<FileInfo>& store) {
    DIR * dirp = opendir(FullName.c_str());
    if (dirp == NULL)
      return;
    struct dirent * dp;
    while ((dp = readdir(dirp)) != NULL) {
      if (dp->d_name[0] == '.' &&
	  (dp->d_namlen == 1 ||
	   (dp->d_namlen == 2 && dp->d_name[1] == '.')))
	continue;

      store.push_back(FileInfo());
      store.back().Name = dp->d_name;
      store.back().FullName = Path::Combine(FullName, dp->d_name);
      store.back().RelativeName = Path::Combine(RelativeName, dp->d_name);
    }
    (void)closedir(dirp);
  }

  void Delete();

  friend class StateEntry;
};

class File
{
public:
  static void Delete(const std::string& path) {
    if (unlink(path.c_str()) == -1)
      throw exception("Failed to delete '" + path + "'");
  }

  static void Copy(const std::string& path, const std::string& dest)
  {
    std::ifstream fin(path.c_str());
    std::ofstream fout(dest.c_str());
    do {
      char buf[8192];
      fin.read(buf, 8192);
      fout.write(buf, fin.gcount());
    } while (! fin.eof() && fin.good() && fout.good());
  }

  static void Move(const std::string& path, const std::string& dest)
  {
    if (rename(path.c_str(), dest.c_str()) == -1)
      throw exception("Failed to move '" + path + "' to '" + dest + "'");
  }

  static void SetOwnership(const std::string& path,
			   mode_t mode, uid_t uid, gid_t gid)
  {
    if (chmod(path.c_str(), mode) == -1)
      throw exception("Failed to change permissions of '" + path + "'");
    if (chown(path.c_str(), uid, gid) == -1)
      throw exception("Failed to change ownership of '" + path + "'");
  }

  static void SetAccessTimes(const std::string& path,
			     struct timespec LastAccessTime,
			     struct timespec LastWriteTime)
  {
    struct timeval temp[2];
    TIMESPEC_TO_TIMEVAL(&temp[0], &LastAccessTime);
    TIMESPEC_TO_TIMEVAL(&temp[1], &LastWriteTime);

    if (utimes(path.c_str(), temp) == -1)
      throw exception("Failed to set last write time of '" + path + "'");
  }
};

class Directory : public File
{
  static void CreateDirectory(const FileInfo& info)
  {
    if (! info.Exists())
      if (mkdir(info.FullName.c_str(), 0755) == -1)
	throw exception("Failed to create directory '" + info.FullName + "'");
  }

public:
  static void CreateDirectory(const std::string& path)
  {
    const char * b = path.c_str();
    const char * p = b + 1;
    while (*p) {
      if (*p == '/')
	CreateDirectory(FileInfo(std::string(path, 0, p - b)));
      ++p;
    }
    CreateDirectory(FileInfo(path));
  }

  static void Delete(const std::string& path)
  {
    if (rmdir(path.c_str()) == -1)
      throw exception("Failed to remove directory '" + path + "'");
  }

  static void Copy(const std::string& path, const std::string& dest)
  {
    assert(0);			// jww (2006-11-03): implement
  }

  static void Move(const std::string& path, const std::string& dest)
  {
    assert(0);			// jww (2006-11-03): implement
  }
};

bool operator==(struct timespec& a, struct timespec& b)
{
  return std::memcmp(&a, &b, sizeof(struct timespec)) == 0;
}

bool operator!=(struct timespec& a, struct timespec& b)
{
  return ! (a == b);
}

void FileInfo::CopyAttributes(FileInfo& dest, bool dataOnly)
{
  if (! dataOnly)
    File::SetOwnership(dest.FullName, Permissions(), OwnerId(), GroupId());

  dest.SetPermissions(Permissions());

  dest.OwnerId() = OwnerId();
  dest.GroupId() = GroupId();

  if (! dataOnly)
    File::SetAccessTimes(dest.FullName, LastAccessTime(), LastWriteTime());

  dest.LastAccessTime() = LastAccessTime();
  dest.LastWriteTime()  = LastWriteTime();
}

void FileInfo::CopyAttributes(const std::string& dest)
{
  File::SetOwnership(dest, Permissions(), OwnerId(), GroupId());
  File::SetAccessTimes(dest, LastAccessTime(), LastWriteTime());
}

void FileInfo::CreateDirectory()
{
  int index = FullName.rfind('/');
  if (index != std::string::npos)
    Directory::CreateDirectory(std::string(FullName, 0, index));
}

void FileInfo::Delete()
{
  if (IsDirectory()) {
    std::list<FileInfo> infos;
    GetFileInfos(infos);
    for (std::list<FileInfo>::iterator i = infos.begin();
	 i != infos.end();
	 i++)
      (*i).Delete();
    Directory::Delete(FullName);
  } else {
    File::Delete(FullName);
  }
}

class StateChange
{
public:
  const bool    DatabaseOnly;
  StateChange * Conflict;

  StateChange(bool databaseOnly)
    : DatabaseOnly(databaseOnly), Conflict(NULL) {}

  virtual void Identify() = 0;
  virtual void Perform() = 0;
  virtual void Report() = 0;
  virtual int  Depth() const = 0;
  virtual void Prepare() {}
};

class StateEntry;
  
class ObjectCreate : public StateChange
{
public:
  StateEntry * dirToCreate;
  StateEntry * dirToImitate;

  // `entryToCopyFrom' lives in the remote repository
  ObjectCreate(bool databaseOnly, StateEntry * _dirToCreate,
	       StateEntry * _dirToImitate)
    : StateChange(databaseOnly), dirToCreate(_dirToCreate),
      dirToImitate(_dirToImitate) {}

  virtual int  Depth() const;
  virtual void Identify();
  virtual void Perform();
  virtual void Report();
};

class ObjectCopy : public StateChange
{
public:
  StateEntry * entryToCopyFrom;
  StateEntry * dirToCopyTo;
  bool         copyDone;

  // `entryToCopyFrom' lives in the remote repository
  ObjectCopy(bool databaseOnly, StateEntry * _entryToCopyFrom,
	     StateEntry * _dirToCopyTo)
    : StateChange(databaseOnly), entryToCopyFrom(_entryToCopyFrom),
      dirToCopyTo(_dirToCopyTo), copyDone(false) {}

  virtual int  Depth() const;
  virtual void Identify();
  virtual void Prepare();
  virtual void Perform();
  virtual void Report();
};

class ObjectUpdate : public StateChange
{
public:
  StateEntry * entryToUpdate;
  StateEntry * entryToUpdateFrom;
  std::string  reason;

  // `entryToUpdateFrom' lives in the remote repository
  ObjectUpdate(bool databaseOnly, StateEntry * _entryToUpdate,
	       StateEntry * _entryToUpdateFrom,
	       const std::string& _reason = "")
    : StateChange(databaseOnly), entryToUpdate(_entryToUpdate),
      entryToUpdateFrom(_entryToUpdateFrom),
      reason(_reason) {}

  virtual int  Depth() const;
  virtual void Identify();
  virtual void Prepare();
  virtual void Perform();
  virtual void Report();
};

class ObjectUpdateProps : public StateChange
{
public:
  StateEntry * entryToUpdate;
  StateEntry * entryToUpdateFrom;
  std::string  reason;

  // `entryToUpdateFrom' lives in the remote repository
  ObjectUpdateProps(bool databaseOnly, StateEntry * _entryToUpdate,
		    StateEntry * _entryToUpdateFrom,
		    const std::string& _reason = "")
    : StateChange(databaseOnly), entryToUpdate(_entryToUpdate),
      entryToUpdateFrom(_entryToUpdateFrom),
      reason(_reason) {}

  virtual int  Depth() const;
  virtual void Identify();
  virtual void Perform();
  virtual void Report();
};

class ObjectDelete : public StateChange
{
public:
  StateEntry * entryToDelete;

  ObjectDelete(bool databaseOnly, StateEntry * _entryToDelete)
    : StateChange(databaseOnly), entryToDelete(_entryToDelete) {}

  virtual int  Depth() const;
  virtual void Identify();
  virtual void Prepare();
  virtual void Perform();
  virtual void Report();
};

void read_binary_string(char *& data, std::string& str)
{
  read_binary_guard(data, 0x3001);

  unsigned char len;
  read_binary_number_nocheck(data, len);
  if (len == 0xff) {
    unsigned short slen;
    read_binary_number_nocheck(data, slen);
    str = std::string(data, slen);
    data += slen;
  }
  else if (len) {
    str = std::string(data, len);
    data += len;
  }
  else {
    str = "";
  }

  read_binary_guard(data, 0x3002);
}

void write_binary_string(std::ostream& out, const std::string& str)
{
  write_binary_guard(out, 0x3001);

  unsigned long len = str.length();
  if (len > 255) {
    assert(len < 65536);
    write_binary_number_nocheck<unsigned char>(out, 0xff);
    write_binary_number_nocheck<unsigned short>(out, len);
  } else {
    write_binary_number_nocheck<unsigned char>(out, len);
  }

  if (len)
    out.write(str.c_str(), len);

  write_binary_guard(out, 0x3002);
}

class FileState;
class StateEntry
{
public:
  FileInfo     Info;
  bool	       Handled;
  bool	       DeletePending;
  int	       Depth;
  static bool  TrustMode;
  FileState *  FileStateObj;
  StateEntry * Parent;

  typedef std::map<std::string, StateEntry *>  children_map;
  typedef std::pair<std::string, StateEntry *> children_pair;

  children_map Children;

  StateEntry(FileState *     _FileStateObj,
	     StateEntry *    _Parent,
	     const FileInfo& _Info = FileInfo())
    : Handled(false), DeletePending(false),
      Depth(_Parent ? _Parent->Depth + 1 : 0),
      FileStateObj(_FileStateObj), Parent(_Parent), Info(_Info)
  {
    if (Parent != NULL)
      Parent->AppendChild(this);
  }

  void AppendChild(StateEntry * entry)
  {
    std::pair<children_map::iterator, bool> result =
      Children.insert(children_pair(entry->Info.Name, entry));
    assert(result.second);
  }

  void WriteTo(std::ostream& w, bool top)
  {
    FileInfo::Kind fileKind = Info.FileKind();

    write_binary_long(w, static_cast<int>(fileKind));

    if (fileKind != FileInfo::Collection) {
      write_binary_string(w, top ? Info.FullName : Info.Name);
      top = false;

      if (fileKind != FileInfo::Nonexistant) {
	write_binary_number(w, Info.LastAccessTime());
	write_binary_number(w, Info.LastWriteTime());
	write_binary_number(w, Info.Permissions());
	write_binary_number(w, Info.OwnerId());
	write_binary_number(w, Info.GroupId());

	if (fileKind == FileInfo::File) {
	  write_binary_long(w, Info.Length());
	  write_binary_number(w, Info.Checksum());
	}
      }
    } else {
      write_binary_long(w, BINARY_VERSION);
    }

    write_binary_long(w, (int)Children.size());
    for (children_map::iterator i = Children.begin();
	 i != Children.end();
	 i++)
      (*i).second->WriteTo(w, top);
  }

  static StateEntry *
  LoadElement(FileState * FileStateObj, StateEntry * Parent,
	      const std::string& parentPath,
	      const std::string& relativePath,
	      char *& data);

  StateEntry * CreateChild(const std::string& name);

  StateEntry * FindChild(const std::string& name)
  {
    for (children_map::iterator i = Children.begin();
	 i != Children.end();
	 i++)
      if ((*i).second->Info.Name == name)
	return (*i).second;

    return NULL;
  }

  StateEntry * FindOrCreateChild(const std::string& name)
  {
    StateEntry * child = FindChild(name);
    if (child == NULL)
      child = CreateChild(name);
    return child;
  }

  void Copy(const std::string& target)
  {
    switch (Info.FileKind()) {
    case FileInfo::File: {
      FileInfo info(target);
      if (info.Exists())
        info.Delete();

      // Copy the entry, and keep the file attributes identical
      File::Copy(Info.FullName, target);
      Info.CopyAttributes(target);
      break;
    }

    case FileInfo::Directory: {
      CopyDirectory(target, true);
      break;
    }

    default:
      assert(false);
      break;
    }
  }

  bool Copy(StateEntry * dirToCopyInto, bool moveOnly);

  void CopyDirectory(const std::string& target, bool andChildren)
  {
    Directory::CreateDirectory(target);

    if (andChildren)
      for (children_map::iterator i = Children.begin();
	   i != Children.end();
	   i++)
	(*i).second->Copy(Path::Combine(target, (*i).second->Info.RelativeName));

    Info.CopyAttributes(target);
  }

  void MarkDeletePending()
  {
    DeletePending = true;

    for (children_map::iterator i = Children.begin();
	 i != Children.end();
	 i++)
      (*i).second->MarkDeletePending();
  }

  void Delete()
  {
    Info.Delete();
  }

  bool Exists() const
  {
    return Info.Exists();
  }

  void Move(const std::string& target)
  {
    switch (Info.FileKind()) {
    case FileInfo::Directory:
      Directory::Move(Info.FullName, target);
      break;

    default:
      File::Move(Info.FullName, target);
      break;
    }

    Info.CopyAttributes(target);
  }

  void CompareTo(StateEntry * other, StateEntry * target = NULL);

  void Report() const
  {
    std::cout << Info.FullName << std::endl;

    for (children_map::const_iterator i = Children.begin();
	 i != Children.end();
	 i++)
      (*i).second->Report();
  }
};

bool StateEntry::TrustMode = true;

struct StateChangeComparer
{
  bool operator()(const StateChange * left, const StateChange * right) const
  {
    bool leftIsCreate	    = dynamic_cast<const ObjectCreate *>(left) != NULL;
    bool leftIsUpdateProps  = (! leftIsCreate &&
			       dynamic_cast<const ObjectUpdateProps *>(left) != NULL);
    bool rightIsCreate      = dynamic_cast<const ObjectCreate *>(right) != NULL;
    bool rightIsUpdateProps = (! rightIsCreate &&
			       dynamic_cast<const ObjectUpdateProps *>(right) != NULL);

    if (leftIsCreate && ! rightIsCreate)
      return true;
    if (! leftIsCreate && rightIsCreate)
      return false;
    if (leftIsUpdateProps && ! rightIsUpdateProps)
      return false;
    if (! leftIsUpdateProps && rightIsUpdateProps)
      return true;

    if (left->DatabaseOnly && ! right->DatabaseOnly)
      return false;
    if (! left->DatabaseOnly && right->DatabaseOnly)
      return true;

    return (right->Depth() - left->Depth()) < 0;
  }
};

class FileState
{
public:
  StateEntry * Root;
  FileInfo *   GenerationPath;

  std::vector<StateChange *> Changes;

  void RegisterUpdate(StateEntry * baseEntryToUpdate,
		      StateEntry * entryToUpdate,
		      StateEntry * entryToUpdateFrom,
		      const std::string& reason = "")
  {
    Changes.push_back(new ObjectUpdate(false, entryToUpdate,
				       entryToUpdateFrom, reason));
    if (baseEntryToUpdate != entryToUpdate)
      baseEntryToUpdate->FileStateObj->Changes.push_back
	(new ObjectUpdate(true, baseEntryToUpdate, entryToUpdateFrom, reason));
  }

  void RegisterUpdateProps(StateEntry * baseEntryToUpdate,
			   StateEntry * entryToUpdate,
			   StateEntry * entryToUpdateFrom,
			   const std::string& reason = "")
  {
    if (entryToUpdate == entryToUpdateFrom)
      return;
    Changes.push_back(new ObjectUpdateProps(false, entryToUpdate,
					    entryToUpdateFrom, reason));
    if (baseEntryToUpdate != entryToUpdate)
      baseEntryToUpdate->FileStateObj->Changes.push_back
	(new ObjectUpdateProps(true, baseEntryToUpdate, entryToUpdateFrom, reason));
  }

  void RegisterDelete(StateEntry * baseEntry, StateEntry * entry)
  {
    if (entry->Info.IsDirectory())
      for (StateEntry::children_map::const_iterator i = entry->Children.begin();
	   i != entry->Children.end();
	   i++) {
	StateEntry * baseChild =
	  (baseEntry != entry ?
	   baseEntry->FindChild((*i).first) : (*i).second);
	RegisterDelete(baseChild, (*i).second);
      }

    Changes.push_back(new ObjectDelete(false, entry));
    if (baseEntry != entry)
      baseEntry->FileStateObj->Changes.push_back(new ObjectDelete(true, baseEntry));
  }

  void RegisterCopy(StateEntry * entry, StateEntry * baseDirToCopyTo,
		    StateEntry * dirToCopyTo)
  {
    if (entry->Info.IsDirectory()) {
      StateEntry * baseSubDir = NULL;
      StateEntry * subDir     = NULL;
      for (StateEntry::children_map::const_iterator i = entry->Children.begin();
	   i != entry->Children.end();
	   i++) {
	if (subDir == NULL) {
	  baseSubDir = subDir =
	    dirToCopyTo->FindOrCreateChild(entry->Info.Name);
	  Changes.push_back(new ObjectCreate(false, subDir, entry));

	  if (baseDirToCopyTo != dirToCopyTo) {
	    baseSubDir = baseDirToCopyTo->FindOrCreateChild(entry->Info.Name);
	    baseDirToCopyTo->FileStateObj->Changes.push_back
	      (new ObjectCreate(true, baseSubDir, entry));
	  }
	}
	RegisterCopy((*i).second, baseSubDir, subDir);
      }
      if (subDir != NULL) {
	Changes.push_back(new ObjectUpdateProps(false, subDir, entry));
	if (baseSubDir != subDir)
	  baseSubDir->FileStateObj->Changes.push_back
	    (new ObjectUpdateProps(true, baseSubDir, entry));
      }
    } else {
      Changes.push_back(new ObjectCopy(false, entry, dirToCopyTo));
      if (baseDirToCopyTo != dirToCopyTo)
	baseDirToCopyTo->FileStateObj->Changes.push_back
	  (new ObjectCopy(true, entry, baseDirToCopyTo));
    }
  }

  typedef std::map<md5sum_t, StateEntry *>  checksum_map;
  typedef std::pair<md5sum_t, StateEntry *> checksum_pair;

  checksum_map ChecksumDict;

  StateEntry * FindDuplicate(StateEntry * child)
  {
    StateEntry * foundEntry = NULL;

    if (! StateEntry::TrustMode && child->Info.FileKind() == FileInfo::File) {
      checksum_map::iterator i = ChecksumDict.find(child->Info.Checksum());
      if (i != ChecksumDict.end())
        foundEntry = (*i).second;
    }

    return foundEntry;
  }

  static FileState * ReadState(const std::list<std::string>& paths,
			       const std::list<Regex *>& IgnoreList,
			       bool verboseOutput)
  {
    FileState * state = new FileState();

    FileInfo info;
    state->Root = new StateEntry(state, NULL, info);

    for (std::list<std::string>::const_iterator i = paths.begin();
	 i != paths.end();
	 i++)
      state->ReadEntry(state->Root, *i, Path::GetFileName(*i), IgnoreList,
		       verboseOutput);

    return state;
  }
    
  static FileState * ReadState(const std::string& path,
			       const std::list<Regex *>& IgnoreList,
			       bool verboseOutput)
  {
    FileState * state = new FileState();

    FileInfo info(path);
    char * data = new char[info.Length() + 1];
    std::ifstream fin(path.c_str());
    fin.read(data, info.Length());
    if (fin.gcount() != info.Length()) {
      delete[] data;
      throw exception("Failed to read database file '" + path + "'");
    }

    try {
      char * ptr = data;
      state->Root = state->LoadElements(ptr, NULL);
    }
    catch (...) {
      delete[] data;
      throw;
    }
    delete[] data;

    return state;
  }

  void BackupEntry(StateEntry * entry)
  {
    if (GenerationPath == NULL)
      return;

    FileInfo target(Path::Combine(GenerationPath->FullName,
				  entry->Info.RelativeName));
    target.CreateDirectory();
    entry->Copy(target.FullName);
  }

  void CompareTo(FileState * other, FileState * target = NULL)
  {
    Root->CompareTo(other->Root, target ? target->Root : NULL);
  }

  void MergeChanges(FileState * other)
  {
    // Look for any conflicting changes between this object's change
    // set and other's change set.

    for (std::vector<StateChange *>::iterator i = other->Changes.begin();
	 i != other->Changes.end();
	 i++)
      Changes.push_back(*i);
  }

  void PerformChanges()
  {
    std::stable_sort(Changes.begin(), Changes.end(), StateChangeComparer());

    for (std::vector<StateChange *>::iterator i = Changes.begin();
	 i != Changes.end();
	 i++) {
      if (dynamic_cast<ObjectCreate *>(*i) != NULL) {
	//(*i)->Identify();
	(*i)->Report();
	(*i)->Perform();
      }
    }

    for (std::vector<StateChange *>::iterator i = Changes.begin();
	 i != Changes.end();
	 i++) {
      //std::cout << "Prepare: "; (*i)->Identify();
      (*i)->Prepare();
    }

    for (std::vector<StateChange *>::iterator i = Changes.begin();
	 i != Changes.end();
	 i++) {
      if (dynamic_cast<ObjectCreate *>(*i) == NULL) {
	//(*i)->Identify();
	(*i)->Report();
	(*i)->Perform();
      }
    }

    Changes.clear();
  }

  void ReportChanges()
  {
    std::stable_sort(Changes.begin(), Changes.end(), StateChangeComparer());

    for (std::vector<StateChange *>::iterator i = Changes.begin();
	 i != Changes.end();
	 i++)
      (*i)->Report();
  }

  StateEntry * LoadElements(char *& data, StateEntry * parent)
  {
    return StateEntry::LoadElement(this, parent, "", "", data);
  }

  void Report() const
  {
    Root->Report();
  }

  void WriteTo(const std::string& path)
  {
    FileInfo info(path);

    info.CreateDirectory();
    if (info.Exists())
      info.Delete();

    std::ofstream fout(path.c_str());
    Root->WriteTo(fout, true);
  }

  StateEntry * ReadEntry(StateEntry * parent, const std::string& path,
			 const std::string& relativePath,
			 const std::list<Regex *>& IgnoreList,
			 bool verboseOutput)
  {
    for (std::list<Regex *>::const_iterator i = IgnoreList.begin();
	 i != IgnoreList.end();
	 i++)
      if ((*i)->IsMatch(path))
	return NULL;

    StateEntry * entry =
      new StateEntry(this, parent, FileInfo(path, relativePath));

    if (entry->Info.FileKind() == FileInfo::Directory)
      ReadDirectory(entry, path, relativePath, IgnoreList, verboseOutput);

    return entry;
  }

  void ReadDirectory(StateEntry * entry, const std::string& path,
		     const std::string& relativePath,
		     const std::list<Regex *>& IgnoreList,
		     bool verboseOutput)
  {
    if (verboseOutput && Path::Parts(relativePath) < 4)
      std::cout << "Scanning: " << entry->Info.FullName << std::endl;

    try {
      std::list<FileInfo> infos;
      entry->Info.GetFileInfos(infos);
      for (std::list<FileInfo>::iterator i = infos.begin();
	   i != infos.end();
	   i++)
        ReadEntry(entry, (*i).FullName, (*i).RelativeName,
		  IgnoreList, verboseOutput);
    }
    catch (...) {
      // Defend against access violations and failures to read!
    }
  }

  static FileState * CreateDatabase(const std::string& path,
				    const std::string& dbfile,
				    bool verbose, bool report)
  {
    std::list<Regex *> ignoreList;
    ignoreList.push_back(new Regex("\\.files\\.db"));

    std::list<std::string> collection;
    collection.push_back(path);

    FileState * tree = FileState::ReadState(collection, ignoreList, verbose);
    if (! dbfile.empty())
      tree->WriteTo(dbfile);

    if (report) {
      std::cout << "created database state:";
      tree->Report();
    }
    return tree;
  }

  FileState * Referent(bool verbose, bool report)
  {
    std::list<std::string> collection;

    for (StateEntry::children_map::iterator i = Root->Children.begin();
	 i != Root->Children.end();
	 i++)
      collection.push_back((*i).second->Info.FullName);

    std::list<Regex *> ignoreList;
    ignoreList.push_back(new Regex("\\.files\\.db"));

    FileState * tree = FileState::ReadState(collection, ignoreList, verbose);
    if (report) {
      std::cout << "read database referent:";
      tree->Report();
    }
    return tree;
  }
};

StateEntry *
StateEntry::LoadElement(FileState *	   FileStateObj,
			StateEntry *	   Parent,
			const std::string& parentPath,
			const std::string& relativePath,
			char *&		   data)
{
  int kind;
  read_binary_long(data, kind);

  FileInfo info;

  info.fileKind = static_cast<FileInfo::Kind>(kind);
  assert(info.fileKind < FileInfo::Last);

  if (kind != FileInfo::Collection) {
    read_binary_string(data, info.Name);
    assert(! info.Name.empty());

    if (info.Name[0] == '/') {
      info.FullName     = info.Name;
      info.Name		= Path::GetFileName(info.FullName);
      info.RelativeName = info.Name;
    } else {
      info.FullName     = Path::Combine(parentPath, info.Name);
      info.RelativeName = Path::Combine(relativePath, info.Name);
    }

    if (kind != FileInfo::Nonexistant) {
      info.didstat = true;

      read_binary_number(data, info.LastAccessTime());
      read_binary_number(data, info.LastWriteTime());
      mode_t mode;
      read_binary_number(data, mode);
      info.SetPermissions(mode);
      read_binary_number(data, info.OwnerId());
      read_binary_number(data, info.GroupId());

      if (kind == FileInfo::File) {
	read_binary_long(data, info.Length());

	info.readcsum = true;
	read_binary_number(data, info.Checksum());
      }
    }
  } else {
    if (read_binary_long<long>(data) != BINARY_VERSION)
      return NULL;
  }

  StateEntry * entry = new StateEntry(FileStateObj, Parent, info);
  if (kind == FileInfo::File)
    FileStateObj->ChecksumDict[info.Checksum()] = entry;

  int children = read_binary_long<int>(data);
  for (int i = 0; i < children; i++)
    LoadElement(FileStateObj, entry, entry->Info.FullName,
		entry->Info.RelativeName, data);

  return entry;
}

StateEntry * StateEntry::CreateChild(const std::string& name)
{
  return new StateEntry(FileStateObj, this,
			FileInfo(Path::Combine(Info.FullName, name),
				 Path::Combine(Info.RelativeName, name)));
}

bool StateEntry::Copy(StateEntry * dirToCopyInto, bool moveOnly)
{
  std::string baseName   = Info.Name;
  std::string targetPath = Path::Combine(dirToCopyInto->Info.FullName, baseName);

  switch (Info.FileKind()) {
  case FileInfo::File: {
    StateEntry * other = dirToCopyInto->FileStateObj->FindDuplicate(this);
    if (other != NULL) {
      if (other->DeletePending && moveOnly) {
	other->FileStateObj->BackupEntry(other);
	other->Move(targetPath);

	other->DeletePending = false;
	StateEntry::children_map::iterator i =
	  other->Parent->Children.find(other->Info.Name);
	assert(i != other->Parent->Children.end());
	other->Parent->Children.erase(i);

	dirToCopyInto->FindOrCreateChild(baseName);
	dirToCopyInto->Info.Reset();
	return true;
      }
      else if (! moveOnly) {
	other->Copy(targetPath);
	dirToCopyInto->FindOrCreateChild(baseName);
	dirToCopyInto->Info.Reset();
	return true;
      }
    }
    else if (! moveOnly) {
      Copy(targetPath);
      dirToCopyInto->FindOrCreateChild(baseName);
      dirToCopyInto->Info.Reset();
      return true;
    }
    break;
  }

  case FileInfo::Directory: {
    if (moveOnly)
      break;

    CopyDirectory(targetPath, false);

    StateEntry * newDir = dirToCopyInto->FindOrCreateChild(baseName);
    for (children_map::iterator i = Children.begin();
	 i != Children.end();
	 i++)
      (*i).second->Copy(newDir, false);

    dirToCopyInto->Info.Reset();
    return true;
  }

  default:
    assert(false);
    break;
  }

  return false;
}

void StateEntry::CompareTo(StateEntry * other, StateEntry * target)
{
  std::string msg;

  FileInfo::Kind fileKind = Info.FileKind();

  if (fileKind != FileInfo::Collection &&
      Info.Name != other->Info.Name)
    throw exception("Names do not match in comparison: " +
		    Info.FullName + " != " + other->Info.Name);

  bool updateRegistered = false;

  StateEntry * goal = target ? target : this;

  if (fileKind != other->Info.FileKind()) {
    if (fileKind != FileInfo::Nonexistant)
      goal->FileStateObj->RegisterDelete(this, goal);
    goal->FileStateObj->RegisterCopy(other, Parent, goal->Parent);
    return;
  }
  else if (fileKind == FileInfo::File) {
    if (Info.Length() != other->Info.Length()) {
      goal->FileStateObj->RegisterUpdate(this, goal, other, "length");
      updateRegistered = true;
    }
    else if (Info.LastWriteTime() != other->Info.LastWriteTime() ||
	     (! TrustMode && Info.Checksum() != other->Info.Checksum())) {
      goal->FileStateObj->RegisterUpdate(this, goal, other, "contents");
      updateRegistered = true;
    }
    else if (Info.Permissions()	!= other->Info.Permissions() ||
	     Info.OwnerId()	!= other->Info.OwnerId() ||
	     Info.GroupId()	!= other->Info.GroupId()) {
      goal->FileStateObj->RegisterUpdateProps(this, goal, this != goal ? goal : other,
					      "attributes");
      updateRegistered = true;
    }
  }
  else if (fileKind != FileInfo::Collection &&
	   fileKind != FileInfo::Nonexistant &&
	   (Info.LastWriteTime() != other->Info.LastWriteTime() ||
	    Info.Permissions()	 != other->Info.Permissions() ||
	    Info.OwnerId()	 != other->Info.OwnerId() ||
	    Info.GroupId()	 != other->Info.GroupId())) {
    goal->FileStateObj->RegisterUpdateProps(this, goal, this != goal ? goal : other,
					    "modtime changed");
    updateRegistered = true;
  }

  bool updateProps = false;

  for (children_map::iterator i = Children.begin();
       i != Children.end();
       i++) {
    StateEntry * goalChild  = target ? goal->FindChild((*i).first) : NULL;
    StateEntry * otherChild = other->FindChild((*i).first);
    if (otherChild != NULL) {
      otherChild->Handled = true;
      if (! target || goalChild)
	(*i).second->CompareTo(otherChild, goalChild);
    } else {
      if (! target || goalChild) {
	FileStateObj->RegisterDelete((*i).second,
				     goalChild ? goalChild : (*i).second);
	updateProps = true;
      }
    }
  }

  for (children_map::iterator i = other->Children.begin();
       i != other->Children.end();
       i++) {
    if ((*i).second->Handled) {
      (*i).second->Handled = false;
    }
    else if (FindChild((*i).first) == NULL) {
      goal->FileStateObj->RegisterCopy((*i).second, this, goal);
      updateProps = true;
    }
  }

  if (updateProps && ! updateRegistered)
    goal->FileStateObj->RegisterUpdateProps(this, goal, other);
}

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

using namespace Attic;

int main(int argc, char *args[])
{
  FileState * comparator    = NULL;
  std::string outputFile;
  std::string databaseFile;
  std::string referenceDir;
  bool	      bidirectional = false;
  bool	      updateRemote  = false;
  bool	      reportFiles   = false;
  bool	      verboseOutput = false;
  bool	      verifyData    = false;
  bool	      cleanFirst    = false;
  std::string generations;

  std::list<std::string> realArgs;
  std::list<Regex *>	 ignoreList;

  try {

  for (int i = 1; i < argc; i++) {
    if (args[i][0] != '-') {
      realArgs.push_back(expand_path(args[i]));
      continue;
    }

    switch (args[i][1]) {
    case 'b':
      bidirectional = true;
      break;

    case 'D':
      reportFiles = true;
      break;

    case 'v':
      verboseOutput = true;
      break;

    case 'V':
      verifyData = true;
      break;

    case 'C':
      cleanFirst = true;
      break;

    case 'g':
      if (i + 1 < argc)
	generations = expand_path(args[++i]);
      break;

    case 'd':
      if (i + 1 < argc)
	databaseFile = expand_path(args[++i]);
      break;

    case 'r':
      if (i + 1 < argc)
	referenceDir = expand_path(args[++i]);
      break;

    case 'u':
      updateRemote = true;
      break;

    case 'p':
      StateEntry::TrustMode = false;
      break;

    case 'o':
      if (i + 1 < argc)
	outputFile = expand_path(args[++i]);
      break;

    case 'x':
      if (i + 1 < argc) {
	FileInfo info(args[i + 1]);
	if (info.FileKind() == FileInfo::File) {
	  std::ifstream fin(args[i + 1]);
	  do {
	    std::string s;
	    std::getline(fin, s);
	    ignoreList.push_back(new Regex(s));
	  } while (! fin.eof() && fin.good());
	} else {
	  ignoreList.push_back(new Regex(args[i + 1]));
	}
	i++;
      }
      break;
    }
  }

  if (databaseFile.empty() && ! referenceDir.empty())
    databaseFile = Path::Combine(referenceDir, ".file_db.xml");

  bool createdDatabase = false;

  if (! databaseFile.empty()) {
    FileInfo dbInfo(databaseFile);
    if (dbInfo.Exists()) {
      std::list<Regex *> temp;
      if (verboseOutput)
	std::cout << "reading database " << databaseFile
		  << "..." << std::endl;
      comparator = FileState::ReadState(dbInfo.FullName, temp, false);
      if (comparator != NULL && reportFiles) {
	std::cout << "read database state:" << std::endl;
	comparator->Report();
      }
    }

    if (comparator == NULL && ! referenceDir.empty()) {
      if (verboseOutput)
	std::cout << "Building database in " << dbInfo.FullName
		  << " ..." << std::endl;
      comparator = FileState::CreateDatabase(referenceDir, dbInfo.FullName,
					     verboseOutput, reportFiles);
      createdDatabase = true;
    }
  }

  if (realArgs.empty() && comparator == NULL) {
    std::cout << "usage: fstate <OPTIONS> [-d DATABASE] [DIRECTORY ...]\n\
\n\
ere options is one or more of:\n\
-C       Delete before updating when reconciling\n\
-D       Turn on debugging (be a lot more verbose)\n\
-d FILE  Specify a database to compare with\n\
-g DIR   When updating, use DIR to keep generational data\n\
-r DIR   Specify a remote directory to reconcile against\n\
-t       Don't use checksum, but trust last write timestamps\n\
-u       Update the given remote directory\n\
-v       Be a bit more verbose\n\
-x FILE  Ignore all entries matching a regexp found in FILE\n\
\n\
re are some of  the most typical forms of usage:\n\
\n\
Copy or update the directory foo to /tmp/foo:\n\
  fstate -u -r /tmp/foo foo\n\
\n\
Compare the directory foo to /tmp/foo:\n\
  fstate -r /tmp/foo foo\n\
\n\
Copy/update foo, but keep state in ~/db.xml:\n\
  fstate -u -d ~/db.xml -r /tmp/foo foo\n\
\n\
Record foo's state in a database for future checking:\n\
  fstate -d ~/db.xml foo\n\
\n\
Check foo's current state against an existing database:\n\
  fstate -d ~/db.xml foo\n\
This will report any alterations made since the database.\n\
\n\
Update the database to reflect foo's recent changes:\n\
  fstate -u -d ~/db.xml foo\n\
\n\
te: Typically when reconciling, fstate optimizes file copies by looking for\n\
ved files on the destination device.  If space is at a premium, however, the\n\
 flag will cause all deletions to occur first." << std::endl;
    return 1;
  }

  if (verboseOutput)
    std::cout << "Reading files ..." << std::endl;

  FileState * referent = NULL;
  FileState * state;
  if (realArgs.empty()) {
    state = comparator->Referent(verboseOutput, reportFiles);
  } else {
    if (bidirectional && comparator != NULL && ! referenceDir.empty())
      referent = comparator->Referent(verboseOutput, reportFiles);

    state = FileState::ReadState(realArgs, ignoreList, verboseOutput);
    if (reportFiles) {
      std::cout << "read file state:" << std::endl;
      state->Report();
    }
  }

  if (comparator != NULL) {
    if (verboseOutput)
      std::cout << "Comparing details ..." << std::endl;
    comparator->CompareTo(state);

    if (referent) {
      if (! comparator->Changes.empty()) {
	if (reportFiles) {
	  std::cout << "Changes between referent and state ..." << std::endl;
	  comparator->ReportChanges();
	}
      }

      comparator->CompareTo(referent, state);
      if (! state->Changes.empty()) {
	if (reportFiles) {
	  std::cout << "Changes between comparator and referent (state relative) ..."
		    << std::endl;
	  state->ReportChanges();
	}
	comparator->MergeChanges(state);
      }
    }

    if (! comparator->Changes.empty()) {
      if (updateRemote) {
	if (! referenceDir.empty()) {
	  std::string genDir;
	  if (updateRemote && ! generations.empty()) {
	    std::time_t now  = std::time(NULL);
	    std::tm *	when = std::localtime(&now);
	    char buf[64];
	    std::strftime(buf, 63, "%Y-%m-%d.%H%M%S", when);

	    genDir = Path::Combine(generations, buf);
	    if (verboseOutput)
	      std::cout << "Backing up to generation directory: " << genDir << std::endl;

	    comparator->GenerationPath = new FileInfo(genDir);
	  }

	  if (verboseOutput)
	    std::cout << "Updating files in " << referenceDir << " ..." << std::endl;
	  comparator->PerformChanges();
	}

	if (! databaseFile.empty()) {
	  if (verboseOutput)
	    std::cout << "Updating database in " << databaseFile << " ..." << std::endl;
	  comparator->WriteTo(databaseFile);
	}

	if (! referenceDir.empty() && verifyData) {
	  if (verboseOutput)
	    std::cout << "Verifying database ..." << std::endl;

	  state = FileState::CreateDatabase(referenceDir, "", verboseOutput,
					    reportFiles);
	  comparator->CompareTo(state);
	  comparator->ReportChanges();
	}
      }
      else if (! createdDatabase) {
	if (verboseOutput)
	  std::cout << "Reporting changes ..." << std::endl;
	comparator->ReportChanges();
      }
    }
  }
  else if (! databaseFile.empty()) {
    if (verboseOutput)
      std::cout << "Updating database in " << databaseFile << " ..." << std::endl;
    state->WriteTo(databaseFile);
  }
  else {
    if (verboseOutput)
      std::cout << "Writing state to database files.db ..." << std::endl;
    state->WriteTo("files.db");
  }

  }
  catch (std::exception& err) {
    std::cerr << "Error: " << err.what() << std::endl;
  }

  return 0;
}
