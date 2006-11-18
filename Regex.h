#ifndef _REGEX_H
#define _REGEX_H

#include <string>

#include <boost/regex.hpp>

class Regex
{
  boost::regex Pattern;

public:
  bool Exclude;

  explicit Regex(const std::string& pattern, bool globStyle = false);

  Regex(const Regex& m) : Exclude(m.Exclude), Pattern(m.Pattern) {}

  bool IsMatch(const std::string& str) const {
    return boost::regex_match(str, Pattern) && ! Exclude;
  }
};

#endif // _REGEX_H
