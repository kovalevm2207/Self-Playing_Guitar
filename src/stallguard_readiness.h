#pragma once
#include <stdint.h>

// Evaluate only fresh PWM readings taken after reaching the requested speed.
struct StallGuardReadiness {
  uint8_t good = 0, bad = 0;
  void reset() { good = bad = 0; }
  void observe(uint8_t pwm) {
    if (pwm >= 250) {
      good = 0;
      if (bad < 255) ++bad;
    } else {
      bad = 0;
      if (good < 255) ++good;
    }
  }
  bool ready(uint32_t stableMs) const { return stableMs >= 300 && good >= 2; }
  bool invalid() const { return bad >= 3; }
};
