#pragma once

#include "hardware/digital_output.h"

namespace guitar {
namespace firmware {

struct StepperTiming {
    uint32_t intervalUs;
    uint16_t pulseUs;
};

class StepperDriver {
public:
    StepperDriver(DigitalOutput& output, uint8_t stepPin, uint8_t directionPin,
                  StepperTiming timing);
    void begin();
    void moveTo(int16_t target, uint32_t now);
    void tick(uint32_t now);
    void stop();
    bool moving() const { return m_position != m_target || m_high; }
    int16_t position() const { return m_position; }

private:
    DigitalOutput& m_output;
    StepperTiming m_timing;
    uint32_t m_next = 0;
    int16_t m_position = 0;
    int16_t m_target = 0;
    uint8_t m_stepPin;
    uint8_t m_directionPin;
    bool m_high = false;
};

} // namespace firmware
} // namespace guitar
