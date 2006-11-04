#ifndef _STATECHANGE_H
#define _STATECHANGE_H

#include <string>
#include <cstddef>

namespace Attic {

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

} // namespace Attic

#endif // _STATECHANGE_H
