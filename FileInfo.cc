#include "FileInfo.h"
#include "Location.h"

namespace Attic {

FileInfo::~FileInfo()
{
  if (Children)
    for (ChildrenMap::iterator i = Children->begin();
	 i != Children->end();
	 i++)
      delete (*i).second;

  if (Parent)
    Parent->RemoveChild(this);

  if (IsTempFile())
    Delete();
}

void FileInfo::SetDetails(const Path& _FullName, FileInfo * _Parent,
			  Location * _Repository)
{
  FullName = _FullName;
  Name     = Path::GetFileName(FullName);

  if (Parent)
    Parent->RemoveChild(this);

  Parent = _Parent;
  if (Parent)
    Parent->InsertChild(this);

  if (Repository)
    Pathname = Repository->SiteBroker->FullPath(FullName);
  else
    Pathname.clear();
}

const md5sum_t& FileInfo::Checksum() const
{
  if (! IsRegularFile())
    throw Exception("Attempt to calc checksum of non-file '" + Moniker() + "'");

  if (! HasFlags(FILEINFO_READCSUM)) {
    Repository->SiteBroker->ComputeChecksum(FullName, csum);
    const_cast<FileInfo&>(*this).SetFlags(FILEINFO_READCSUM);
  }
  return csum;
}

md5sum_t FileInfo::CurrentChecksum() const
{
  if (! IsRegularFile())
    throw Exception("Attempt to calc checksum of non-file '" + Moniker() + "'");

  md5sum_t temp;
  Repository->SiteBroker->ComputeChecksum(FullName, temp);
  return temp;
}

void FileInfo::ReadAttributes()
{
  if (! HasFlags(FILEINFO_READATTR)) {
    Repository->SiteBroker->ReadAttributes(*this);
    SetFlags(FILEINFO_READATTR);
  }
}

bool FileInfo::Exists() const
{
  const_cast<FileInfo&>(*this).ReadAttributes();
  return HasFlags(FILEINFO_EXISTS);
}

void FileInfo::Create()
{
  if (! Exists()) {
    if (Parent)
      Parent->Create();
    Repository->SiteBroker->Create(*this);
  }
}

void FileInfo::CreateDirectory() const
{
  if (Parent)
    Parent->Create();
  else
    Repository->SiteBroker->CreateDirectory(DirectoryName());
}

void FileInfo::Delete()
{
  Repository->SiteBroker->Delete(*this);
}

void FileInfo::Copy(const Path& dest) const
{
  Repository->SiteBroker->Copy(*this, dest);
}

void FileInfo::Move(const Path& dest)
{
  Repository->SiteBroker->Move(*this, dest);
}

void FileInfo::CopyAttributes(FileInfo& dest) const
{
  dest.SetFlags(Flags() | FILEINFO_READATTR);

  if (HasFlags(FILEINFO_READCSUM))
    dest.SetChecksum(Checksum());

  CopyAttributes(dest.FullName);
}

void FileInfo::CopyAttributes(const Path& dest) const
{
  Repository->SiteBroker->CopyAttributes(*this, dest);
}

FileInfo::ChildrenMap::size_type FileInfo::ChildrenSize() const
{
  if (! IsDirectory())
    throw Exception("Attempt to call ChildrenSize on a non-directory");

  if (! Children) {
    Children = new ChildrenMap;
    Repository->SiteBroker->ReadDirectory(const_cast<FileInfo&>(*this));
  }
  return Children->size();
}

FileInfo::ChildrenMap::iterator FileInfo::ChildrenBegin() const
{
  if (! IsDirectory())
    throw Exception("Attempt to call ChildrenBegin on a non-directory");

  if (! Children) {
    Children = new ChildrenMap;
    Repository->SiteBroker->ReadDirectory(const_cast<FileInfo&>(*this));
  }
  return Children->begin();
}

FileInfo * FileInfo::CreateChild(const std::string& name)
{
  assert(! name.empty());
  return Repository->SiteBroker->CreateFileInfo(Path::Combine(FullName, name), this);
}

void FileInfo::InsertChild(FileInfo * entry)
{
  if (! Children)
    Children = new ChildrenMap;

  std::pair<ChildrenMap::iterator, bool> result =
    Children->insert(ChildrenPair(entry->Name, entry));
  assert(result.second);
}

void FileInfo::RemoveChild(FileInfo * child)
{
  if (Children)
    Children->erase(child->Name);
}

FileInfo * FileInfo::FindChild(const std::string& name)
{
  assert(! name.empty());

  if (! Children)
    return NULL;

  for (ChildrenMap::iterator i = ChildrenBegin();
       i != ChildrenEnd();
       i++)
    if ((*i).first == name)
      return (*i).second;

  return NULL;
}

FileInfo * FileInfo::FindOrCreateChild(const std::string& name)
{
  FileInfo * child = FindChild(name);
  if (child == NULL)
    child = CreateChild(name);
  return child;
}

FileInfo * FileInfo::FindMember(const Path& path)
{
  if (path.empty())
    return this;

  FileInfo * current = this;

  std::string::size_type previ = 0;
  for (std::string::size_type index = path.find('/', previ);
       index != std::string::npos;
       previ = index + 1, index = path.find('/', previ)) {
    std::string part(path, previ, index - previ);
    current = current->FindChild(part);
    if (! current)
      return NULL;
  }

  return current->FindChild(std::string(path, previ));
}

FileInfo * FileInfo::FindOrCreateMember(const Path& path)
{
  if (path.empty())
    return this;

  FileInfo * current = this;

  std::string::size_type previ = 0;
  for (std::string::size_type index = path.find('/', previ);
       index != std::string::npos;
       previ = index + 1, index = path.find('/', previ)) {
    std::string part(path, previ, index - previ);
    current = current->FindOrCreateChild(part);
  }

  return current->FindOrCreateChild(std::string(path, previ));
}

std::string FileInfo::Moniker() const
{
  return Repository->SiteBroker->Moniker(*this);
}

} // namespace Attic
