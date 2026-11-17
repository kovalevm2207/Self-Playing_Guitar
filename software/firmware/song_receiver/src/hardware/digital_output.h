#pragma once

#include <stdint.h>

namespace guitar {
namespace firmware {

class DigitalOutput {
public:
    virtual ~DigitalOutput() = default;
    virtual void configure(uint8_t pin) = 0;
    virtual void write(uint8_t pin, bool high) = 0;
};

// Virtual lines have no connection to the board's GPIO numbers.
class SimulatedOutput : public DigitalOutput {
public:
    void configure(uint8_t) override {}
    void write(uint8_t pin, bool high) override {
        if (pin < 32) {
            const uint32_t mask = uint32_t(1) << pin;
            m_levels = high ? m_levels | mask : m_levels & ~mask;
        }
    }
    bool high(uint8_t pin) const {
        return pin < 32 && (m_levels & (uint32_t(1) << pin));
    }

private:
    uint32_t m_levels = 0;
};

} // namespace firmware
} // namespace guitar
