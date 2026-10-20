#include <Arduino.h>
#include <avr/interrupt.h>

// Uno: STEP=D2, DIR=D3, EN=D4. Timer1 is reserved for this test.
// Test only an unloaded, freely rotating shaft. Set the current limit first.
constexpr uint8_t STEP_PIN = 2, DIR_PIN = 3, EN_PIN = 4;
// D2 requires software pulses. Keep ISR load bounded to preserve serial STOP.
// Physical pulse timing at the ceiling must be checked with a scope.
constexpr uint32_t MAX_RATE = 100000;
constexpr uint32_t RAMP_INTERVAL_MS = 20;
constexpr uint32_t ACCELERATION = 500; // pulses/s per second
constexpr uint32_t MIN_SPEED_INCREMENT = 200;
constexpr uint32_t HOLD_MS = 4000;
constexpr uint32_t LINK_TIMEOUT_MS = 2000;
bool running = false;
uint32_t rate = 0, target = 200;
uint32_t generatedRate = 0;
uint32_t lastHeartbeat = 0, lastRamp = 0, settledAt = 0, lastLog = 0;

ISR(TIMER1_COMPA_vect) {
  PORTD |= _BV(PD2);
  delayMicroseconds(2);
  PORTD &= ~_BV(PD2);
}

void setRate(uint32_t pulsesPerSecond) {
  // CTC compare interrupt once per STEP period, 16MHz / 8 = 2MHz.
  const uint32_t timerClock = F_CPU / 8UL;
  // Round up the period so generated frequency never exceeds the request.
  const uint32_t ticks = (timerClock + pulsesPerSecond - 1) / pulsesPerSecond;
  const uint16_t compare = static_cast<uint16_t>(ticks - 1);
  const uint8_t timerMode = _BV(WGM12) | _BV(CS11);
  const uint8_t saved = SREG;
  cli();
  const bool clockChanged = TCCR1B != timerMode;
  if (clockChanged) TCCR1B = 0;
  OCR1A = compare;
  if (clockChanged || TCNT1 > compare) TCNT1 = 0;
  TIFR1 = _BV(OCF1A);
  TCCR1A = 0;
  TIMSK1 = _BV(OCIE1A);
  TCCR1B = timerMode;
  SREG = saved;
  // Integer timer periods quantize the frequency, especially at high speeds.
  generatedRate = timerClock / ticks;
}

void stopTest() {
  const uint8_t saved = SREG;
  cli();
  TCCR1B = 0;
  TCCR1A = 0;
  TIMSK1 = 0;
  PORTD &= ~_BV(PD2);
  SREG = saved;
  digitalWrite(EN_PIN, HIGH);
  running = false;
  Serial.print(F("LAST commanded_pulses_s=")); Serial.println(generatedRate);
  rate = 0;
  Serial.println(F("STOPPED; driver disabled; shaft may coast"));
}

void startTest() {
  if (running) return;
  target = 200;
  rate = 100;
  settledAt = 0;
  lastHeartbeat = lastRamp = millis();
  digitalWrite(DIR_PIN, HIGH);
  digitalWrite(EN_PIN, LOW);
  delayMicroseconds(100);
  running = true;
  setRate(rate);
  Serial.println(F("RUNNING; STEP=D2; ramp=500 pulses/s^2; increment=min200/10%; hold=4s; limit=100000 pulses/s"));
}

void setup() {
  digitalWrite(EN_PIN, HIGH);
  pinMode(EN_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  TCCR1A = 0;
  TCCR1B = 0; // Timer is stopped until START.
  TIMSK1 = 0;
  Serial.begin(115200);
  Serial.println(F("READY SPEED V2; STEP=D2 DIR=D3 EN=D4; s=start x=stop h=heartbeat; timeout=2s"));
}

void loop() {
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == 'x') stopTest();
    else if (c == 's') startTest();
    else if (c == 'h') lastHeartbeat = millis();
  }
  const uint32_t now = millis();
  if (!running) return;
  if (now - lastHeartbeat >= LINK_TIMEOUT_MS) {
    stopTest();
    Serial.println(F("LINK TIMEOUT"));
    return;
  }
  if (now - lastRamp >= RAMP_INTERVAL_MS) {
    lastRamp = now;
    if (rate < target) {
      rate = min(rate + ACCELERATION * RAMP_INTERVAL_MS / 1000, target);
      setRate(rate);
      settledAt = 0;
    } else if (!settledAt) {
      settledAt = now;
    } else if (now - settledAt >= HOLD_MS) {
      if (target == MAX_RATE) {
        stopTest();
        Serial.println(F("DONE; UNO GENERATOR LIMIT reached, NOT a measured motor maximum"));
        return;
      }
      target = min(target + max(MIN_SPEED_INCREMENT, target / 10), MAX_RATE);
      settledAt = 0;
    }
  }
  if (now - lastLog >= 500) {
    lastLog = now;
    Serial.print(F("pulse_rate=")); Serial.print(generatedRate);
    Serial.print(F(" pulses/s; target=")); Serial.println(target);
  }
}
