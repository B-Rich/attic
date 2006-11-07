#include "DateTime.h"
#include "error.h"

#include <ctime>
#include <cctype>

#include <iostream>

#if defined __FreeBSD__ && __FreeBSD__ <=4
// FreeBSD has a broken isspace macro, so dont use it
#undef isspace(c)
#endif

#if defined(__GNUG__) && __GNUG__ < 3
namespace std {
  inline ostream & right (ostream & i) {
    i.setf(i.right, i.adjustfield);
    return i;
  }
  inline ostream & left (ostream & i) {
    i.setf(i.left, i.adjustfield);
    return i;
  }
}
typedef unsigned long istream_pos_type;
typedef unsigned long ostream_pos_type;
#else
typedef std::istream::pos_type istream_pos_type;
typedef std::ostream::pos_type ostream_pos_type;
#endif

Date	    Date::Now(std::time(NULL));
int	    Date::current_year	= Date::Now.year();
std::string Date::input_format;
std::string Date::output_format = "%Y/%m/%d";

const char * Date::formats[] = {
  "%Y/%m/%d",
  "%m/%d",
  "%Y.%m.%d",
  "%m.%d",
  "%Y-%m-%d",
  "%m-%d",
  "%a",
  "%A",
  "%b",
  "%B",
  "%Y",
  NULL
};

DateTime DateTime::Now(std::time(NULL));

namespace {
  static std::time_t base = -1;
  static int base_year = -1;

  static const int month_days[12] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
  };

  bool parse_date_mask(const char * date_str, struct std::tm * result)
  {
    if (! Date::input_format.empty()) {
      std::memset(result, INT_MAX, sizeof(struct std::tm));
      if (strptime(date_str, Date::input_format.c_str(), result))
	return true;
    }
    for (const char ** f = Date::formats; *f; f++) {
      std::memset(result, INT_MAX, sizeof(struct std::tm));
      if (strptime(date_str, *f, result))
	return true;
    }
    return false;
  }

  bool parse_date(const char * date_str, std::time_t * result, const int year)
  {
    struct std::tm when;

    if (! parse_date_mask(date_str, &when))
      return false;

    when.tm_hour = 0;
    when.tm_min  = 0;
    when.tm_sec  = 0;

    if (when.tm_year == -1)
      when.tm_year = ((year == -1) ? Date::current_year : (year - 1900));

    if (when.tm_mon == -1)
      when.tm_mon = 0;

    if (when.tm_mday == -1)
      when.tm_mday = 1;

    *result = std::mktime(&when);

    return true;
  }

  inline bool quick_parse_date(const char * date_str, std::time_t * result)
  {
    return parse_date(date_str, result, Date::current_year + 1900);
  }
}

Date::Date(const std::string& _when)
{
  if (! quick_parse_date(_when.c_str(), &when))
    throw Exception(std::string("Invalid date string: ") + _when);
}

inline char peek_next_nonws(std::istream& in) {
  char c = in.peek();
  while (! in.eof() && std::isspace(c)) {
    in.get(c);
    c = in.peek();
  }
  return c;
}

#define READ_INTO(str, targ, size, var, cond) {				\
  char * _p = targ;							\
  var = str.peek();							\
  while (! str.eof() && var != '\n' && (cond) && _p - targ < size) {	\
    str.get(var);							\
    if (str.eof())							\
      break;								\
    if (var == '\\') {							\
      str.get(var);							\
      if (in.eof())							\
	break;								\
    }									\
    *_p++ = var;							\
    var = str.peek();							\
  }									\
  *_p = '\0';								\
}

#define READ_INTO_(str, targ, size, var, idx, cond) {			\
  char * _p = targ;							\
  var = str.peek();							\
  while (! str.eof() && var != '\n' && (cond) && _p - targ < size) {	\
    str.get(var);							\
    if (str.eof())							\
      break;								\
    idx++;								\
    if (var == '\\') {							\
      str.get(var);							\
      if (in.eof())							\
	break;								\
      idx++;								\
    }									\
    *_p++ = var;							\
    var = str.peek();							\
  }									\
  *_p = '\0';								\
}

void Date::parse(std::istream& in)
{
  char buf[256];
  char c = peek_next_nonws(in);
  READ_INTO(in, buf, 255, c,
	    std::isalnum(c) || c == '-' || c == '.' || c == '/');

  if (! quick_parse_date(buf, &when))
    throw Exception(std::string("Invalid date string: ") + buf);
}

DateTime::DateTime(const std::string& _when)
{
  std::istringstream datestr(_when);
  parse(datestr);		// parse both the date and optional time
}

void DateTime::parse(std::istream& in)
{
  Date::parse(in);		// first grab the date part

  istream_pos_type beg_pos = in.tellg();

  int hour = 0;
  int min  = 0;
  int sec  = 0;

  // Now look for the (optional) time specifier.  If no time is given,
  // we use midnight of the given day.
  char buf[256];
  char c = peek_next_nonws(in);
  if (! std::isdigit(c))
    goto abort;
  READ_INTO(in, buf, 255, c, std::isdigit(c));
  if (buf[0] == '\0')
    goto abort;

  hour = std::atoi(buf);
  if (hour > 23)
    goto abort;

  if (in.peek() == ':') {
    in.get(c);
    READ_INTO(in, buf, 255, c, std::isdigit(c));
    if (buf[0] == '\0')
      goto abort;

    min = std::atoi(buf);
    if (min > 59)
      goto abort;

    if (in.peek() == ':') {
      in.get(c);
      READ_INTO(in, buf, 255, c, std::isdigit(c));
      if (buf[0] == '\0')
	goto abort;

      sec = std::atoi(buf);
      if (sec > 59)
	goto abort;
    }
  }

  c = peek_next_nonws(in);
  if (c == 'a' || c == 'p' || c == 'A' || c == 'P') {
    if (hour > 12)
      goto abort;
    in.get(c);

    if (c == 'p' || c == 'P') {
      if (hour != 12)
	hour += 12;
    } else {
      if (hour == 12)
	hour = 0;
    }

    c = in.peek();
    if (c == 'm' || c == 'M')
      in.get(c);
  }

  struct std::tm * desc = std::localtime(&when);

  desc->tm_hour  = hour;
  desc->tm_min   = min;
  desc->tm_sec   = sec;
  desc->tm_isdst = -1;

  when = std::mktime(desc);

  return;			// the time has been successfully parsed

 abort:				// there was no valid time string to parse
  in.clear();
  in.seekg(beg_pos, std::ios::beg);
}

std::ostream& operator<<(std::ostream& out, const DateTime& moment)
{
  std::string format = DateTime::output_format;
  std::tm * when = moment.localtime();
  if (when->tm_hour != 0 || when->tm_min != 0 || when->tm_sec != 0)
    format += " %H:%M:%S";

  char buf[64];
  std::strftime(buf, 63, format.c_str(), when);
  out << buf;
  return out;
}
