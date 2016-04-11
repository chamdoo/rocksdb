#include <iostream>
#include <cstdlib>

namespace std {
  int stoi (const string& str, size_t* idx = 0, int base = 10) {
    char* endptr;
    int num = std::strtoul(str.c_str(), &endptr, base);
    *idx = endptr - str.c_str();
    return num;
  }

  int stod (const string& str, size_t* idx = 0) {
    char* endptr;
    int num = std::strtod(str.c_str(), &endptr);
    *idx = endptr - str.c_str();
    return num;
  }

  unsigned long long stoull (const string& str, size_t* idx = 0, int base = 10) {
    char* endptr;
    unsigned long long num = std::strtoul(str.c_str(), &endptr, base);
    *idx = endptr - str.c_str();
    return num;
  }
}
