#pragma once
#include <sstream>
#include <stdint.h>
#include <string>
using __FlashStringHelper = char;
#define F(text) text
#define LOW 0
#define HIGH 1
#define OUTPUT 1
inline unsigned gpioWrites = 0;
inline void digitalWrite(uint8_t, uint8_t) {
    ++gpioWrites;
}
inline void pinMode(uint8_t, uint8_t) {}
struct MockSerial {
    std::string input, output;
    void begin(int) {}
    size_t available() const { return input.size(); }
    char read() {
        char c = input.front();
        input.erase(0, 1);
        return c;
    }
    template <class T>
    void print(T value) {
        std::ostringstream s;
        s << value;
        output += s.str();
    }
    template <class T>
    void println(T value) {
        print(value);
        output += '\n';
    }
};
inline MockSerial Serial;
inline unsigned long testTime = 0;
inline unsigned long millis() {
    return testTime;
}
inline uint32_t micros() {
    return uint32_t(testTime * 1000);
}
