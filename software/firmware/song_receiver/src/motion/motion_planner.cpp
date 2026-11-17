#include "motion/motion_planner.h"

#include "hardware/solenoid_driver.h"

namespace guitar {
namespace firmware {

bool MotionPlanner::plan(const Note& note, int16_t currentSteps,
                         NotePlan& result) const {
    if (!valid(note) || !(m_config.stringMask & (1 << (note.string - 1))) ||
        m_config.stepper.intervalUs <= m_config.stepper.pulseUs ||
        !m_config.stepper.pulseUs) {
        return false;
    }
    // Open strings do not require the carriage to move to the nut.
    result.targetSteps = note.fret ? m_config.fretSteps[note.fret] : currentSteps;
    const int32_t delta = int32_t(result.targetSteps) - currentSteps;
    const uint32_t steps = delta < 0 ? -delta : delta;
    const uint64_t move = steps ? uint64_t(steps) * m_config.stepper.intervalUs +
                                      m_config.stepper.pulseUs
                                : 0;
    const uint64_t preparation = move + m_config.releaseUs + m_config.settleUs +
                                 (note.fret ? m_config.clampUs : 0);
    if (preparation > 10000000UL || !m_config.pickUs ||
        m_config.pickUs > 1000000UL || m_config.maxLatenessUs > 1000000UL ||
        uint64_t(note.duration) * 1000 + m_config.clampUs + m_config.maxLatenessUs >
            SolenoidDriver::MaxActivationUs) {
        return false;
    }
    result.moveUs = uint32_t(move);
    result.prepareUs = uint32_t(preparation);
    return true;
}

bool MotionPlanner::validate(const Note* notes, uint16_t count,
                             const int16_t (&positions)[6]) const {
    if (!notes || !count || count > MaxNotes) {
        return false;
    }
    int16_t predicted[6];
    for (uint8_t i = 0; i < 6; ++i) {
        predicted[i] = positions[i];
    }
    for (uint16_t i = 0; i < count; ++i) {
        const auto& note = notes[i];
        if (!valid(note)) {
            return false;
        }
        NotePlan next{};
        if (!plan(note, predicted[note.string - 1], next) ||
            m_config.pickUs > uint32_t(note.duration) * 1000 ||
            (i && next.prepareUs + m_config.pickUs >
                      uint32_t(notes[i - 1].duration) * 1000)) {
            return false;
        }
        predicted[note.string - 1] = next.targetSteps;
    }
    return true;
}

} // namespace firmware
} // namespace guitar
