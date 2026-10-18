#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
#include <cmath>
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define pgm_read_word(p) (*(const uint16_t*)(p))
#define pgm_read_dword(p) (*(const uint32_t*)(p))
class __FlashStringHelper;
class String : public std::string {
public:
    using std::string::string;
    unsigned int length() const { return size(); }
};
inline void yield() {}
inline double radians(double degrees) { return degrees * 3.141592653589793 / 180; }
