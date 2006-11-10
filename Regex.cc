#include "Regex.h"
#include "error.h"

Regex::Regex(const std::string& pat, bool globStyle) : Exclude(false)
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

  std::string cpat;

  if (! globStyle) {
    cpat = p;
  } else {
    bool hadSlash = false;
    for (const char * q = p; *q; q++)
      if (*q == '/') {
	hadSlash = true;
	break;
      }

    if (! hadSlash)
      cpat += "^";

    while (*p) {
      switch (*p) {
      case '?':
	cpat += '.';
	break;

      case '*':
	if (*(p + 1) == '*')
	  cpat += ".*";
	else
	  cpat += "[^/]*";
	break;

      case '+':
      case '.':
      case '$':
      case '^':
	cpat += '\\';
	cpat += *p;
	break;

      default:
	cpat += *p;
	break;
      }
      ++p;
    }
    cpat += "$";
  }

  Pattern.assign(cpat);
}
