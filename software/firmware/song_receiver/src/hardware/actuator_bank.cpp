#include "hardware/actuator_bank.h"

namespace guitar {
namespace firmware {

ActuatorBank::ActuatorBank(DigitalOutput& output, StepperTiming timing)
    : m_axes{{output, 0, 1, timing},
             {output, 4, 5, timing},
             {output, 8, 9, timing},
             {output, 12, 13, timing},
             {output, 16, 17, timing},
             {output, 20, 21, timing}},
      m_clamps{{output, 2},
               {output, 6},
               {output, 10},
               {output, 14},
               {output, 18},
               {output, 22}},
      m_picks{{output, 3},
              {output, 7},
              {output, 11},
              {output, 15},
              {output, 19},
              {output, 23}} {}

void ActuatorBank::begin() {
    for (uint8_t i = 0; i < 6; ++i) {
        m_axes[i].begin();
        m_clamps[i].begin();
        m_picks[i].begin();
    }
}

void ActuatorBank::tick(uint32_t now) {
    for (uint8_t i = 0; i < 6; ++i) {
        m_axes[i].tick(now);
        m_clamps[i].tick(now);
        m_picks[i].tick(now);
    }
}

void ActuatorBank::releaseClamps() {
    for (auto& clamp : m_clamps) {
        clamp.release();
    }
}

void ActuatorBank::stop() {
    for (uint8_t i = 0; i < 6; ++i) {
        m_axes[i].stop();
        m_clamps[i].release();
        m_picks[i].release();
    }
}

} // namespace firmware
} // namespace guitar
