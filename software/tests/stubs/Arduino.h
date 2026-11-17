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
inline uint8_t gpioLevels[32]{};
inline uint8_t gpioModes[32]{};
inline void digitalWrite(uint8_t pin, uint8_t level) {
    ++gpioWrites;
    gpioLevels[pin] = level;
}
inline void pinMode(uint8_t pin, uint8_t mode) {
    gpioModes[pin] = mode;
}
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
