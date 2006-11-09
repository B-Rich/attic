#ifndef _REGEX_H
#define _REGEX_H

#include <string>

#include <pcre.h>

class Regex
{
  void * regexp;

public:
  bool Exclude;
  std::string Pattern;

  explicit Regex(const std::string& pattern,
		 bool globStyle = false);
  Regex(const Regex&);
  ~Regex();

  bool IsMatch(const std::string& str) const;
};

#endif // _REGEX_H
