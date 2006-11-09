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

  if (globStyle) {
    bool hadSlash = false;
    for (const char * q = p; *q; q++)
      if (*q == '/') {
	hadSlash = true;
	break;
      }

    if (! hadSlash)
      Pattern += "^";

    while (*p) {
      switch (*p) {
      case '?':
	Pattern += '.';
	break;

      case '*':
	if (*(p + 1) == '*')
	  Pattern += ".*";
	else
	  Pattern += "[^/]*";
	break;

      case '+':
      case '.':
      case '$':
      case '^':
	Pattern += '\\';
	Pattern += *p;
	break;

      default:
	Pattern += *p;
	break;
      }
      ++p;
    }
    Pattern += "$";
  } else {
    Pattern = p;
  }

  const char *error;
  int erroffset;
  regexp = pcre_compile(Pattern.c_str(), PCRE_CASELESS,
			&error, &erroffset, NULL);
  if (! regexp)
    throw Exception(std::string("Failed to compile regexp '") + Pattern + "'");
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
