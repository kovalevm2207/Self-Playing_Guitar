#pragma once

#include <Arduino.h>

#include "hardware/digital_output.h"

namespace guitar {
namespace firmware {

// Instantiate only with a calibrated board profile and a real-time pulse clock.
// The default firmware uses SimulatedOutput and never constructs this adapter.
class ArduinoOutput : public DigitalOutput {
public:
    void configure(uint8_t pin) override {
        digitalWrite(pin, LOW);
        pinMode(pin, OUTPUT);
    }
    void write(uint8_t pin, bool high) override {
        digitalWrite(pin, high ? HIGH : LOW);
    }
};

} // namespace firmware
} // namespace guitar
