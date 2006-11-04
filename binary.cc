#include "binary.h"

void read_binary_string(char *& data, std::string& str)
{
  read_binary_guard(data, 0x3001);

  unsigned char len;
  read_binary_number_nocheck(data, len);
  if (len == 0xff) {
    unsigned short slen;
    read_binary_number_nocheck(data, slen);
    str = std::string(data, slen);
    data += slen;
  }
  else if (len) {
    str = std::string(data, len);
    data += len;
  }
  else {
    str = "";
  }

  read_binary_guard(data, 0x3002);
}

void write_binary_string(std::ostream& out, const std::string& str)
{
  write_binary_guard(out, 0x3001);

  unsigned long len = str.length();
  if (len > 255) {
    assert(len < 65536);
    write_binary_number_nocheck<unsigned char>(out, 0xff);
    write_binary_number_nocheck<unsigned short>(out, len);
  } else {
    write_binary_number_nocheck<unsigned char>(out, len);
  }

  if (len)
    out.write(str.c_str(), len);

  write_binary_guard(out, 0x3002);
}
