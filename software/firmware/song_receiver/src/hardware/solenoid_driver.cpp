#include "hardware/solenoid_driver.h"

namespace guitar {
namespace firmware {

void SolenoidDriver::begin() {
    m_output.configure(m_pin);
    release();
}

void SolenoidDriver::activate(uint32_t now, uint32_t durationUs) {
    if (!durationUs || durationUs > MaxActivationUs) {
        release();
        return;
    }
    m_deadline = now + durationUs;
    m_active = true;
    m_output.write(m_pin, true);
}

void SolenoidDriver::tick(uint32_t now) {
    if (m_active && int32_t(now - m_deadline) >= 0) {
        release();
    }
}

void SolenoidDriver::release() {
    m_output.write(m_pin, false);
    m_active = false;
}

} // namespace firmware
} // namespace guitar
