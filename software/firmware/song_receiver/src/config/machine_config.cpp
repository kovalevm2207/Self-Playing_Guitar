#include "config/machine_config.h"

namespace guitar {
namespace firmware {

MachineConfig simulationConfig(uint8_t stringMask) {
    MachineConfig config{};
    for (uint8_t fret = 0; fret <= 24; ++fret) {
        config.fretSteps[fret] = fret * 2;
    }
    config.stepper = {1000, 20};
    config.releaseUs = 1000;
    config.settleUs = 1000;
    config.clampUs = 1000;
    config.pickUs = 2000;
    config.maxLatenessUs = 5000;
    config.stringMask = stringMask;
    return config;
}

} // namespace firmware
} // namespace guitar
