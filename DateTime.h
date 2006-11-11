#ifndef _DATETIME_H
#define _DATETIME_H

#include <ctime>
#include <sstream>

#include <sys/types.h>
#include <sys/time.h>

class DateTime
{
protected:
  static const char * formats[];
  static int	      current_year;
  static std::string  input_format;
  static std::string  output_format;

public:
  std::time_t secs;
  long	      nsecs;

  static DateTime Now;

  DateTime() {}
  DateTime(const DateTime& when) : secs(when.secs), nsecs(when.nsecs) {}
  DateTime(const std::time_t& _secs, const long _nsecs = 0)
    : secs(_secs), nsecs(_nsecs) {}
  DateTime(const struct timeval& when)
    : secs(when.tv_sec), nsecs(when.tv_usec * 1000) {}
  DateTime(const struct timespec& when)
    : secs(when.tv_sec), nsecs(when.tv_nsec) {}
  DateTime(const std::string& timestr);

  DateTime& operator=(const DateTime& when) {
    secs  = when.secs;
    nsecs = when.nsecs;
    return *this;
  }
  DateTime& operator=(const std::time_t _secs) {
    secs  = _secs;
    nsecs = 0;
    return *this;
  }
  DateTime& operator=(const struct timeval& when) {
    secs  = when.tv_sec;
    nsecs = when.tv_usec * 1000;
    return *this;
  }
  DateTime& operator=(const struct timespec& when) {
    secs  = when.tv_sec;
    nsecs = when.tv_nsec;
    return *this;
  }
  DateTime& operator=(const std::string& timestr) {
    return *this = DateTime(timestr);
  }

  long operator-=(const DateTime& date) {
    return secs - date.secs;
  }

  virtual DateTime& operator+=(const long _secs) {
    secs += _secs;
    return *this;
  }
  virtual DateTime& operator-=(const long _secs) {
    secs -= _secs;
    return *this;
  }

#define DEF_DATETIME_OP(OP)					\
  bool operator OP(const DateTime& other) const {		\
    return (secs OP other.secs ||				\
	    (secs == other.secs && nsecs OP other.nsecs));	\
  }

  DEF_DATETIME_OP(<)
  DEF_DATETIME_OP(<=)
  DEF_DATETIME_OP(>)
  DEF_DATETIME_OP(>=)
  DEF_DATETIME_OP(==)
  DEF_DATETIME_OP(!=)

  std::tm * localtime() const {
    return std::localtime(&secs);
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
  int hour() const {
    return localtime()->tm_hour;
  }
  int min() const {
    return localtime()->tm_min;
  }
  int sec() const {
    return localtime()->tm_sec;
  }

  operator bool() const {
    return secs != 0 && nsecs != 0;
  }
  operator std::time_t() const {
    return secs;
  }
  operator struct timeval() const {
    struct timeval temp;
    temp.tv_sec	 = secs;
    temp.tv_usec = nsecs / 1000;
    return temp;
  }
  operator struct timespec() const {
    struct timespec temp;
    temp.tv_sec	 = secs;
    temp.tv_nsec = nsecs;
    return temp;
  }
  operator std::string() const {
    return to_string();
  }

  void parse(std::istream& in);
  void write(std::ostream& out,
	     const std::string& format = output_format) const {
    out << to_string(format);
  }

  std::string to_string(const std::string& format = output_format) const {
    char buf[64];
    std::strftime(buf, 63, format.c_str(), localtime());
    return buf;
  }

  friend std::ostream& operator<<(std::ostream& out, const DateTime& moment);

protected:
  static bool parse_date_mask(const char * date_str, struct std::tm * result);
  static bool parse_date(const char * date_str, std::time_t * result, const int year);
  static bool quick_parse_date(const char * date_str, std::time_t * result) {
    return parse_date(date_str, result, DateTime::current_year + 1900);
  }
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

#endif // _DATETIME_H
