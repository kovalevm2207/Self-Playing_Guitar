#include "playback/playback_controller.h"

namespace guitar {
namespace firmware {

bool PlaybackController::busy() const {
    return m_phase == PlaybackPhase::Releasing || m_phase == PlaybackPhase::Moving ||
           m_phase == PlaybackPhase::Settling ||
           m_phase == PlaybackPhase::Clamping || m_phase == PlaybackPhase::Sounding;
}

void PlaybackController::fail(const char* error) {
    m_actuators.stop();
    m_phase = PlaybackPhase::Fault;
    m_error = error;
}

void PlaybackController::stop() {
    m_actuators.stop();
    m_phase = PlaybackPhase::Stopped;
    m_error = "";
    m_notes = nullptr;
    m_index = 0;
    m_count = 0;
}

bool PlaybackController::prepare(uint32_t scheduledStart) {
    const auto& note = m_notes[m_index];
    if (!m_planner.plan(note, m_actuators.axis(note.string).position(), m_plan)) {
        fail("PLAN");
        return false;
    }
    m_actuators.releaseClamps();
    m_deadline = scheduledStart + m_planner.config().releaseUs;
    m_phase = PlaybackPhase::Releasing;
    return true;
}

bool PlaybackController::play(const Note* notes, uint16_t count, uint32_t now) {
    if (busy()) {
        return false;
    }
    int16_t positions[6];
    for (uint8_t string = 1; string <= 6; ++string) {
        positions[string - 1] = m_actuators.axis(string).position();
    }
    if (!m_planner.validate(notes, count, positions)) {
        fail("PLAN");
        return false;
    }
    m_notes = notes;
    m_count = count;
    m_index = 0;
    m_error = "";
    if (!prepare(now)) {
        return false;
    }
    m_onset = now + m_plan.prepareUs;
    return true;
}

void PlaybackController::tick(uint32_t now) {
    if (busy() && int32_t(now - m_deadline) >= 0 &&
        uint32_t(now - m_deadline) > m_planner.config().maxLatenessUs) {
        fail("TIMING");
        return;
    }
    m_actuators.tick(now);
    // Bounded transitions allow zero-duration phases without blocking Serial.
    for (uint8_t transitions = 0; busy() && transitions < 6; ++transitions) {
        if (int32_t(now - m_deadline) < 0) {
            return;
        }
        const auto& config = m_planner.config();
        if (uint32_t(now - m_deadline) > config.maxLatenessUs) {
            fail("TIMING");
            return;
        }
        const auto& note = m_notes[m_index];
        switch (m_phase) {
            case PlaybackPhase::Releasing:
                m_actuators.axis(note.string).moveTo(m_plan.targetSteps, now);
                m_deadline += m_plan.moveUs;
                m_phase = PlaybackPhase::Moving;
                break;
            case PlaybackPhase::Moving:
                if (m_actuators.axis(note.string).moving()) {
                    return;
                }
                m_deadline += config.settleUs;
                m_phase = PlaybackPhase::Settling;
                break;
            case PlaybackPhase::Settling:
                if (note.fret) {
                    m_actuators.clamp(note.string)
                        .activate(now,
                                  uint32_t(note.duration) * 1000 + config.clampUs +
                                      config.maxLatenessUs);
                }
                m_deadline = m_onset;
                m_phase = PlaybackPhase::Clamping;
                break;
            case PlaybackPhase::Clamping: {
                m_actuators.pick(note.string).activate(now, config.pickUs);
                m_deadline = m_onset + uint32_t(note.duration) * 1000;
                if (m_index + 1 < m_count) {
                    const auto& next = m_notes[m_index + 1];
                    NotePlan plan{};
                    if (!m_planner.plan(
                            next, m_actuators.axis(next.string).position(), plan)) {
                        fail("PLAN");
                        return;
                    }
                    m_deadline -= plan.prepareUs;
                }
                m_phase = PlaybackPhase::Sounding;
                break;
            }
            case PlaybackPhase::Sounding:
                if (m_index + 1 == m_count) {
                    m_actuators.stop();
                    m_phase = PlaybackPhase::Completed;
                    return;
                }
                m_onset += uint32_t(note.duration) * 1000;
                ++m_index;
                if (!prepare(m_deadline)) {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

} // namespace firmware
} // namespace guitar
