#ifndef _BROKER_H
#define _BROKER_H

#include <string>

namespace Attic {

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

} // namespace Attic

#endif // _BROKER_H
