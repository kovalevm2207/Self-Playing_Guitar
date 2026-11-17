#pragma once

#include "hardware/stepper_driver.h"

namespace guitar {
namespace firmware {

struct MachineConfig {
    int16_t fretSteps[25];
    StepperTiming stepper;
    uint32_t releaseUs;
    uint32_t settleUs;
    uint32_t clampUs;
    uint32_t pickUs;
    uint32_t maxLatenessUs;
    uint8_t stringMask;
};

// Deliberately artificial geometry and timing, exclusively for simulation.
// A hardware profile needs measured coordinates, homing and a timer backend.
MachineConfig simulationConfig();

} // namespace firmware
} // namespace guitar
