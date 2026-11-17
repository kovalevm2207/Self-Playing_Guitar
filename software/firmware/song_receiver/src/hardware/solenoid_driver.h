#pragma once

#include "hardware/digital_output.h"

namespace guitar {
namespace firmware {

class SolenoidDriver {
public:
    static constexpr uint32_t MaxActivationUs = 11000000UL;
    SolenoidDriver(DigitalOutput& output, uint8_t pin)
        : m_output(output), m_pin(pin) {}
    void begin();
    void activate(uint32_t now, uint32_t durationUs);
    void tick(uint32_t now);
    void release();
    bool active() const { return m_active; }

private:
    DigitalOutput& m_output;
    uint32_t m_deadline = 0;
    uint8_t m_pin;
    bool m_active = false;
};

} // namespace firmware
} // namespace guitar
