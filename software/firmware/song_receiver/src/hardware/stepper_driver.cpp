#include "hardware/stepper_driver.h"

namespace guitar {
namespace firmware {

StepperDriver::StepperDriver(DigitalOutput& output, uint8_t stepPin,
                             uint8_t directionPin, StepperTiming timing)
    : m_output(output), m_timing(timing), m_stepPin(stepPin),
      m_directionPin(directionPin) {}

void StepperDriver::begin() {
    m_output.configure(m_stepPin);
    m_output.configure(m_directionPin);
    stop();
}

void StepperDriver::moveTo(int16_t target, uint32_t now) {
    stop();
    if (!m_timing.pulseUs || m_timing.intervalUs <= m_timing.pulseUs) {
        return;
    }
    m_target = target;
    m_output.write(m_directionPin, target > m_position);
    // One interval before the first edge also establishes the direction signal.
    m_next = now + m_timing.intervalUs;
}

void StepperDriver::tick(uint32_t now) {
    if (!moving() || int32_t(now - m_next) < 0) {
        return;
    }
    if (m_high) {
        m_output.write(m_stepPin, false);
        m_high = false;
        m_next = now + m_timing.intervalUs - m_timing.pulseUs;
    } else {
        m_output.write(m_stepPin, true);
        m_high = true;
        m_position += m_target > m_position ? 1 : -1;
        m_next = now + m_timing.pulseUs;
    }
}

void StepperDriver::stop() {
    m_output.write(m_stepPin, false);
    m_high = false;
    m_target = m_position;
}

} // namespace firmware
} // namespace guitar
