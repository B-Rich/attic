#include "StateChange.h"
#include "Location.h"

namespace Attic {

void StateChange::Report(MessageLog& log) const
{
  std::string prefix;

  switch (ChangeKind) {
  case Add:
    prefix = "A ";
    break;
  case Remove:
    prefix = "R ";
    break;
  case Update:
    prefix = "M ";
    break;
  case UpdateProps:
    prefix = "p ";
    break;
  default:
    assert(0);
    break;
  }

  LOG(log, Message, prefix << Item->Moniker());
}

void StateChange::DebugPrint(MessageLog& log) const
{
  std::string label;

  switch (ChangeKind) {
  case Nothing:	    label = "Nothing "; break;
  case Add:	    label = "Add "; break;
  case Remove:	    label = "Remove "; break;
  case Update:	    label = "Update "; break;
  case UpdateProps: label = "UpdateProps "; break;
  default:          assert(0); break;
  }

  if (Ancestor) {
    LOG(log, Message,
	label << "Ancestor(" << Ancestor << ") " <<
	Item->FullName << " ");
  } else {
    LOG(log, Message, label << Item->FullName << " ");
  }
}

int StateChange::Depth() const
{
  assert(Ancestor);
  return 0;
  //return Ancestor->Depth + (ChangeKind == Add ? 1 : 0);
}

} // namespace Attic
