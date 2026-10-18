#pragma once
#include "Arduino.h"
class Print {
public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t) = 0;
    size_t print(const char* text) { size_t n=0; while (*text) n+=write(*text++); return n; }
    size_t print(const String& text) { return print(text.c_str()); }
};
