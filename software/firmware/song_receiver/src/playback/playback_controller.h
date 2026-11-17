#pragma once

#include "hardware/actuator_bank.h"
#include "motion/motion_planner.h"

namespace guitar {
namespace firmware {

enum class PlaybackPhase : uint8_t {
    Idle,
    Releasing,
    Moving,
    Settling,
    Clamping,
    Sounding,
    Completed,
    Stopped,
    Fault
};

class PlaybackController {
public:
    PlaybackController(const MotionPlanner& planner, ActuatorBank& actuators)
        : m_planner(planner), m_actuators(actuators) {}
    bool play(const Note* notes, uint16_t count, uint32_t now);
    void tick(uint32_t now);
    void stop();
    bool busy() const;
    PlaybackPhase phase() const { return m_phase; }
    uint16_t noteIndex() const { return m_index; }
    const char* error() const { return m_error; }
    uint8_t supportedStringMask() const { return m_planner.config().stringMask; }

private:
    bool prepare(uint32_t scheduledStart);
    void fail(const char* error);

    const MotionPlanner& m_planner;
    ActuatorBank& m_actuators;
    const Note* m_notes = nullptr;
    NotePlan m_plan{};
    const char* m_error = "";
    uint32_t m_deadline = 0;
    uint32_t m_onset = 0;
    uint16_t m_count = 0;
    uint16_t m_index = 0;
    PlaybackPhase m_phase = PlaybackPhase::Idle;
};

} // namespace firmware
} // namespace guitar
