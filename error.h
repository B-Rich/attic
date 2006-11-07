#ifndef _ERROR_H
#define _ERROR_H

#include <exception>
#include <string>

class Exception : public std::exception
{
protected:
  std::string reason;

public:
  Exception(const std::string& _reason) throw()
    : reason(_reason) {}
  virtual ~Exception() throw() {}

  virtual const char* what() const throw() {
    return reason.c_str();
  }
};

extern bool DebugMode;

#endif // _ERROR_H
