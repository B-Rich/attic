#ifndef _BINARY_H
#define _BINARY_H

#include <string>
#include <iostream>

namespace Attic {

template <typename T>
inline void read_binary_number_nocheck(std::istream& in, T& num) {
  in.read((char *)&num, sizeof(num));
}

template <typename T>
inline void read_binary_number_nocheck(char *& data, T& num) {
  num = *((T *) data);
  data += sizeof(T);
}

template <typename T>
inline T read_binary_number_nocheck(std::istream& in) {
  T num;
  read_binary_number_nocheck(in, num);
  return num;
}

template <typename T>
inline T read_binary_number_nocheck(char *& data) {
  T num;
  read_binary_number_nocheck(data, num);
  return num;
}

#define read_binary_guard(in, id)

template <typename T>
inline void read_binary_number(std::istream& in, T& num) {
  read_binary_guard(in, 0x2003);
  in.read((char *)&num, sizeof(num));
  read_binary_guard(in, 0x2004);
}

template <typename T>
inline void read_binary_number(char *& data, T& num) {
  read_binary_guard(data, 0x2003);
  num = *((T *) data);
  data += sizeof(T);
  read_binary_guard(data, 0x2004);
}

template <typename T>
inline T read_binary_number(std::istream& in) {
  T num;
  read_binary_number(in, num);
  return num;
}

template <typename T>
inline T read_binary_number(char *& data) {
  T num;
  read_binary_number(data, num);
  return num;
}

void read_binary_bool(std::istream& in, bool& num);
void read_binary_bool(char *& data, bool& num);

inline bool read_binary_bool(std::istream& in) {
  bool num;
  read_binary_bool(in, num);
  return num;
}

inline bool read_binary_bool(char *& data) {
  bool num;
  read_binary_bool(data, num);
  return num;
}

template <typename T>
void read_binary_long(std::istream& in, T& num)
{
  read_binary_guard(in, 0x2001);

  unsigned char len;
  read_binary_number_nocheck(in, len);

  num = 0;
  unsigned char temp;
  if (len > 3) {
    read_binary_number_nocheck(in, temp);
    num |= ((unsigned long)temp) << 24;
  }
  if (len > 2) {
    read_binary_number_nocheck(in, temp);
    num |= ((unsigned long)temp) << 16;
  }
  if (len > 1) {
    read_binary_number_nocheck(in, temp);
    num |= ((unsigned long)temp) << 8;
  }

  read_binary_number_nocheck(in, temp);
  num |= ((unsigned long)temp);

  read_binary_guard(in, 0x2002);
}

template <typename T>
void read_binary_long(char *& data, T& num)
{
  read_binary_guard(data, 0x2001);

  unsigned char len;
  read_binary_number_nocheck(data, len);

  num = 0;
  unsigned char temp;
  if (len > 3) {
    read_binary_number_nocheck(data, temp);
    num |= ((unsigned long)temp) << 24;
  }
  if (len > 2) {
    read_binary_number_nocheck(data, temp);
    num |= ((unsigned long)temp) << 16;
  }
  if (len > 1) {
    read_binary_number_nocheck(data, temp);
    num |= ((unsigned long)temp) << 8;
  }

  read_binary_number_nocheck(data, temp);
  num |= ((unsigned long)temp);

  read_binary_guard(data, 0x2002);
}

template <typename T>
inline T read_binary_long(std::istream& in) {
  T num;
  read_binary_long(in, num);
  return num;
}

template <typename T>
inline T read_binary_long(char *& data) {
  T num;
  read_binary_long(data, num);
  return num;
}

void read_binary_string(std::istream& in, std::string& str);
void read_binary_string(char *& data, std::string& str);
void read_binary_string(char *& data, std::string * str);

inline std::string read_binary_string(std::istream& in) {
  std::string temp;
  read_binary_string(in, temp);
  return temp;
}

inline std::string read_binary_string(char *& data) {
  std::string temp;
  read_binary_string(data, temp);
  return temp;
}


template <typename T>
inline void write_binary_number_nocheck(std::ostream& out, T num) {
  out.write((char *)&num, sizeof(num));
}

#define write_binary_guard(in, id)

template <typename T>
inline void write_binary_number(std::ostream& out, T num) {
  write_binary_guard(out, 0x2003);
  out.write((char *)&num, sizeof(num));
  write_binary_guard(out, 0x2004);
}

void write_binary_bool(std::ostream& out, bool num);

template <typename T>
void write_binary_long(std::ostream& out, T num)
{
  write_binary_guard(out, 0x2001);

  unsigned char len = 4;
  if (((unsigned long)num) < 0x00000100UL)
    len = 1;
  else if (((unsigned long)num) < 0x00010000UL)
    len = 2;
  else if (((unsigned long)num) < 0x01000000UL)
    len = 3;
  write_binary_number_nocheck<unsigned char>(out, len);

  unsigned char temp;
  if (len > 3) {
    temp = (((unsigned long)num) & 0xFF000000UL) >> 24;
    write_binary_number_nocheck(out, temp);
  }
  if (len > 2) {
    temp = (((unsigned long)num) & 0x00FF0000UL) >> 16;
    write_binary_number_nocheck(out, temp);
  }
  if (len > 1) {
    temp = (((unsigned long)num) & 0x0000FF00UL) >> 8;
    write_binary_number_nocheck(out, temp);
  }

  temp = (((unsigned long)num) & 0x000000FFUL);
  write_binary_number_nocheck(out, temp);

  write_binary_guard(out, 0x2002);
}

void write_binary_string(std::ostream& out, const std::string& str);

} // namespace ledger

#endif // _BINARY_H
