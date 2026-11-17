#pragma once

#include "hardware/solenoid_driver.h"
#include "hardware/stepper_driver.h"

namespace guitar {
namespace firmware {

// Six virtual channels: STEP, DIR, clamp and pick. These are simulation line IDs.
class ActuatorBank {
public:
    ActuatorBank(DigitalOutput& output, StepperTiming timing);
    void begin();
    void tick(uint32_t now);
    void stop();
    void releaseClamps();
    StepperDriver& axis(uint8_t string) { return m_axes[string - 1]; }
    SolenoidDriver& clamp(uint8_t string) { return m_clamps[string - 1]; }
    SolenoidDriver& pick(uint8_t string) { return m_picks[string - 1]; }

private:
    StepperDriver m_axes[6];
    SolenoidDriver m_clamps[6];
    SolenoidDriver m_picks[6];
};

} // namespace firmware
} // namespace guitar
