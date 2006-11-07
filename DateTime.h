#ifndef _DATETIME_H
#define _DATETIME_H

#include <ctime>
#include <sstream>

class DateTime;
class Date
{
  Date(const DateTime& _when);

 protected:
  std::time_t when;

 public:
  static Date         Now;
  static const char * formats[];
  static int	      current_year;
  static std::string  input_format;
  static std::string  output_format;

  Date() : when(0) {}
  Date(const Date& _when) : when(_when.when) {}

  Date(const std::time_t _when) : when(_when) {
#if 0
    struct std::tm * moment = std::localtime(&_when);
    moment->tm_hour = 0;
    moment->tm_min  = 0;
    moment->tm_sec  = 0;
    when = std::mktime(moment);
#endif
  }
  Date(const std::string& _when);

  virtual ~Date() {}

  Date& operator=(const Date& _when) {
    when = _when.when;
    return *this;
  }
  Date& operator=(const std::time_t _when) {
    return *this = Date(_when);
  }
  Date& operator=(const DateTime& _when) {
    return *this = Date(_when);
  }
  Date& operator=(const std::string& _when) {
    return *this = Date(_when);
  }

  long operator-=(const Date& date) {
    return (when - date.when) / 86400;
  }

  virtual Date& operator+=(const long days) {
    // jww (2006-03-26): This is not accurate enough when DST is in effect!
    assert(0);
    when += days * 86400;
    return *this;
  }
  virtual Date& operator-=(const long days) {
    assert(0);
    when -= days * 86400;
    return *this;
  }

#define DEF_DATE_OP(OP)				\
  bool operator OP(const Date& other) const {	\
    return when OP other.when;			\
  }

  DEF_DATE_OP(<)
  DEF_DATE_OP(<=)
  DEF_DATE_OP(>)
  DEF_DATE_OP(>=)
  DEF_DATE_OP(==)
  DEF_DATE_OP(!=)

  operator bool() const {
    return when != 0;
  }
  operator std::time_t() const {
    return when;
  }
  operator std::string() const {
    return to_string();
  }

  std::string to_string(const std::string& format = output_format) const {
    char buf[64];
    std::strftime(buf, 63, format.c_str(), localtime());
    return buf;
  }

  int year() const {
    return localtime()->tm_year + 1900;
  }
  int month() const {
    return localtime()->tm_mon + 1;
  }
  int day() const {
    return localtime()->tm_mday;
  }
  int wday() const {
    return localtime()->tm_wday;
  }

  std::tm * localtime() const {
    return std::localtime(&when);
  }

  void write(std::ostream& out,
	     const std::string& format = output_format) const {
    out << to_string(format);
  }

  void parse(std::istream& in);

  friend class DateTime;
};

inline long operator-(const Date& left, const Date& right) {
  Date temp(left);
  temp -= right;
  return temp;
}

inline Date operator+(const Date& left, const long days) {
  Date temp(left);
  temp += days;
  return temp;
}

inline Date operator-(const Date& left, const long days) {
  Date temp(left);
  temp -= days;
  return temp;
}

inline std::ostream& operator<<(std::ostream& out, const Date& moment) {
  moment.write(out);
  return out;
}

inline std::istream& operator>>(std::istream& in, Date& moment) {
  moment.parse(in);
  return in;
}

class DateTime : public Date
{
 public:
  static DateTime Now;

  DateTime() : Date() {}
  DateTime(const DateTime& _when) : Date(_when.when) {}
  DateTime(const Date& _when) : Date(_when) {}

  DateTime(const std::time_t _when) : Date(_when) {}
  DateTime(const std::string& _when);

  DateTime& operator=(const DateTime& _when) {
    when = _when.when;
    return *this;
  }
  DateTime& operator=(const Date& _when) {
    when = _when.when;
    return *this;
  }
  DateTime& operator=(const std::time_t _when) {
    return *this = DateTime(_when);
  }
  DateTime& operator=(const std::string& _when) {
    return *this = DateTime(_when);
  }

  long operator-=(const DateTime& date) {
    return when - date.when;
  }

  virtual DateTime& operator+=(const long secs) {
    when += secs;
    return *this;
  }
  virtual DateTime& operator-=(const long secs) {
    when -= secs;
    return *this;
  }

#define DEF_DATETIME_OP(OP)				\
  bool operator OP(const DateTime& other) const {	\
    return when OP other.when;				\
  }

  DEF_DATETIME_OP(<)
  DEF_DATETIME_OP(<=)
  DEF_DATETIME_OP(>)
  DEF_DATETIME_OP(>=)
  DEF_DATETIME_OP(==)
  DEF_DATETIME_OP(!=)

  int hour() const {
    return localtime()->tm_hour;
  }
  int min() const {
    return localtime()->tm_min;
  }
  int sec() const {
    return localtime()->tm_sec;
  }

  void parse(std::istream& in);
};

inline long operator-(const DateTime& left, const DateTime& right) {
  std::time_t left_time(left);
  std::time_t right_time(right);
  return left_time - right_time;
}

inline DateTime operator+(const DateTime& left, const long seconds) {
  DateTime temp(left);
  temp += seconds;
  return temp;
}

inline DateTime operator-(const DateTime& left, const long seconds) {
  DateTime temp(left);
  temp -= seconds;
  return temp;
}

std::ostream& operator<<(std::ostream& out, const DateTime& moment);

inline std::istream& operator>>(std::istream& in, DateTime& moment) {
  moment.parse(in);
  return in;
}

inline Date::Date(const DateTime& _when) {
  assert(0);
  struct std::tm * moment = _when.localtime();
  moment->tm_hour = 0;
  moment->tm_min  = 0;
  moment->tm_sec  = 0;
  when = std::mktime(moment);
}

#endif // _DATETIME_H
