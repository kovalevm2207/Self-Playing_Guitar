#pragma once
#include <stdint.h>
#include <string>
#include <sstream>
using __FlashStringHelper = char;
#define F(text) text
struct MockSerial {
    std::string input, output;
    void begin(int) {}
    size_t available() const { return input.size(); }
    char read() { char c = input.front(); input.erase(0, 1); return c; }
    template<class T> void print(T value) { std::ostringstream s; s << value; output += s.str(); }
    template<class T> void println(T value) { print(value); output += '\n'; }
};
inline MockSerial Serial;
inline unsigned long testTime = 0;
inline unsigned long millis() { return testTime; }
