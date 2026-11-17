#pragma once

#include "config/machine_config.h"
#include "score.h"

namespace guitar {
namespace firmware {

struct NotePlan {
    int16_t targetSteps;
    uint32_t moveUs;
    uint32_t prepareUs;
};

class MotionPlanner {
public:
    explicit MotionPlanner(const MachineConfig& config) : m_config(config) {}
    bool plan(const Note& note, int16_t currentSteps, NotePlan& result) const;
    bool validate(const Note* notes, uint16_t count,
                  const int16_t (&positions)[6]) const;
    const MachineConfig& config() const { return m_config; }

private:
    const MachineConfig& m_config;
};

} // namespace firmware
} // namespace guitar
