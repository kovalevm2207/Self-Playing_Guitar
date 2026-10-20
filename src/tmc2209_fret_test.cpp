#include <Arduino.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <math.h>
#include <stdlib.h>

// GT2 20T = 40 mm/rev; 1/8 = 1600 STEP/rev = 40 STEP/mm.
// No homing or end switches: establish clearance/reference manually first.
constexpr uint8_t STEP_PIN = 2, DIR_PIN = 3, EN_PIN = 4;
constexpr uint8_t MOVE_COUNT = 6; // Three round trips at the chosen distance.
constexpr uint8_t MAX_SEGMENTS = 128;
constexpr uint32_t BEAT_MS = 375; // 160 BPM, one move per quarter note.
constexpr uint32_t SETTLE_MS = 75;
uint16_t segmentPulses[MAX_SEGMENTS], segmentTicks[MAX_SEGMENTS];
uint8_t segmentClock[MAX_SEGMENTS];
uint8_t segmentCount = 0;
uint16_t totalPulses = 5000, distanceMm = 125;
volatile uint16_t pulsesSent = 0, timerLate = 0, segmentLeft = 0;
volatile uint8_t segmentIndex = 0;
volatile bool reverseProfile = false;
volatile bool moving = false, moveFinished = false;
volatile uint32_t finishedAtUs = 0;
bool session = false, forward = true;
uint8_t movesDone = 0;
uint16_t durationMs = 800;
uint32_t heartbeat = 0, startedAtUs = 0, nextMoveAt = 0;
char commandBuffer[24];
uint8_t commandLength = 0;
bool receivingCommand = false;

float crossingTime(uint16_t step) {
  const float position = static_cast<float>(step) / totalPulses;
  // Only first half is generated; the other half is mirrored in the ISR.
  if (position <= 3.0F / 14.0F) return sqrtf(position * 0.42F);
  return 0.3F + (position - 3.0F / 14.0F) * 0.7F;
}

void buildProfile() {
  segmentCount = 0;
  uint16_t previous = 0;
  float previousTime = 0;
  for (uint16_t i = 1; i <= MAX_SEGMENTS; ++i) {
    // Fine distance intervals near the beginning of acceleration.
    const uint16_t boundary = static_cast<uint32_t>(i) * i * (totalPulses / 2)
        / (static_cast<uint32_t>(MAX_SEGMENTS) * MAX_SEGMENTS);
    if (boundary == previous) continue;
    const float endTime = crossingTime(boundary);
    const uint16_t count = boundary - previous;
    const float ticks = (endTime - previousTime) * durationMs * 2000.0F / count;
    const bool slow = ticks > 65535.0F;
    segmentPulses[segmentCount] = count;
    segmentTicks[segmentCount] = static_cast<uint16_t>((slow ? ticks / 8 : ticks) + 0.5F);
    segmentClock[segmentCount] = slow ? _BV(CS11) | _BV(CS10) : _BV(CS11);
    ++segmentCount;
    previous = boundary;
    previousTime = endTime;
  }
}

void selectInterval() {
  const uint16_t ticks = segmentTicks[segmentIndex];
  const uint8_t mode = _BV(WGM12) | segmentClock[segmentIndex];
  if (TCCR1B != mode) {
    TCCR1B = 0;
    TCNT1 = 0;
  } else if (TCNT1 >= ticks) {
    TCNT1 = 0;
    ++timerLate;
  }
  OCR1A = ticks - 1;
  TCCR1B = mode;
}

ISR(TIMER1_COMPA_vect) {
  PORTD |= _BV(PD2);
  delayMicroseconds(2);
  PORTD &= ~_BV(PD2);
  ++pulsesSent;
  if (pulsesSent == totalPulses) {
    TCCR1B = 0;
    TIMSK1 = 0;
    finishedAtUs = micros();
    moving = false;
    moveFinished = true;
    return;
  }
  if (--segmentLeft == 0) {
    if (!reverseProfile) {
      if (segmentIndex + 1 < segmentCount) ++segmentIndex;
      else reverseProfile = true;
    } else --segmentIndex;
    segmentLeft = segmentPulses[segmentIndex];
    selectInterval();
  }
}

void stopTest() {
  uint16_t count;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    TCCR1B = 0;
    TIMSK1 = 0;
    PORTD &= ~_BV(PD2);
    moving = moveFinished = false;
    count = pulsesSent;
  }
  session = false;
  digitalWrite(EN_PIN, HIGH);
  Serial.print(F("STOPPED; last_move_pulses=")); Serial.println(count);
  Serial.println(F("Position not verified; after abort/stall restore reference manually"));
}

void beginMove() {
  digitalWrite(DIR_PIN, forward ? HIGH : LOW);
  digitalWrite(EN_PIN, LOW);
  delayMicroseconds(100);
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    TCCR1B = 0;
    pulsesSent = timerLate = 0;
    moveFinished = false;
    segmentIndex = 0;
    reverseProfile = false;
    segmentLeft = segmentPulses[0];
    TCCR1A = 0;
    OCR1A = segmentTicks[0] - 1;
    TCNT1 = 0;
    TIFR1 = _BV(OCF1A);
    moving = true;
    startedAtUs = micros();
    TIMSK1 = _BV(OCIE1A);
    TCCR1B = _BV(WGM12) | segmentClock[0];
  }
}

void startSession(uint16_t distance, uint16_t duration) {
  if (session) { Serial.println(F("ERROR busy")); return; }
  if (distance < 1 || distance > 300 || duration < 300 || duration > 800) {
    Serial.println(F("ERROR range: distance=1..300 mm; duration=300..800 ms"));
    return;
  }
  distanceMm = distance;
  totalPulses = distance * 40;
  durationMs = duration;
  buildProfile();
  movesDone = 0;
  forward = true;
  heartbeat = millis();
  const float seconds = durationMs / 1000.0F;
  const float peakMmS = distanceMm / (0.7F * seconds);
  Serial.print(F("PLAN distance_mm=")); Serial.print(distanceMm);
  Serial.print(F("; pulses=")); Serial.print(totalPulses);
  Serial.print(F("; moves=6; move_ms=")); Serial.print(durationMs);
  Serial.print(F("; peak_rpm=")); Serial.print(peakMmS * 60.0F / 40.0F, 1);
  Serial.print(F("; accel_mm_s2=")); Serial.println(peakMmS / (0.3F * seconds), 0);
  Serial.println(F("These are commanded values, NOT measured shaft motion"));
  session = true;
  // First move begins after the main loop has drained serial input.
  nextMoveAt = millis() + 20;
}

void setup() {
  digitalWrite(EN_PIN, HIGH);
  pinMode(EN_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  TCCR1A = TCCR1B = TIMSK1 = 0;
  Serial.begin(115200);
  Serial.println(F("FRET READY V2; STEP=D2 DIR=D3 EN=D4; GT2 20T; microstep=8"));
  Serial.println(F("Mdistance,duration followed by newline; x=stop; h=heartbeat"));
}

void loop() {
  while (Serial.available()) {
    const char command = Serial.read();
    if (command == 'h') heartbeat = millis();
    else if (command == 'x') { receivingCommand = false; stopTest(); }
    else if (command == 'M') { receivingCommand = true; commandLength = 0; }
    else if (receivingCommand && command == '\n') {
      receivingCommand = false;
      commandBuffer[commandLength] = '\0';
      char *separator;
      const unsigned long distance = strtoul(commandBuffer, &separator, 10);
      if (separator == commandBuffer || *separator != ',') { Serial.println(F("ERROR format")); continue; }
      char *end;
      const unsigned long duration = strtoul(separator + 1, &end, 10);
      if (end == separator + 1 || *end || distance < 1 || distance > 300 || duration < 300 || duration > 800) {
        Serial.println(F("ERROR invalid distance/duration"));
        continue;
      }
      startSession(distance, duration);
    } else if (receivingCommand && command != '\r') {
      if (commandLength < sizeof(commandBuffer) - 1 && ((command >= '0' && command <= '9') || command == ','))
        commandBuffer[commandLength++] = command;
      else { receivingCommand = false; Serial.println(F("ERROR invalid command")); }
    }
  }
  if (millis() - heartbeat >= 2000) {
    if (session) { stopTest(); Serial.println(F("LINK TIMEOUT")); }
    else digitalWrite(EN_PIN, HIGH);
    return;
  }
  if (!session) return;
  if (moveFinished) {
    uint32_t endUs;
    uint16_t late;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      endUs = finishedAtUs;
      late = timerLate;
      moveFinished = false;
    }
    ++movesDone;
    Serial.print(F("MOVE ")); Serial.print(movesDone);
    Serial.print(F("/6; direction=")); Serial.print(forward ? F("OUT") : F("BACK"));
    Serial.print(F("; pulses=")); Serial.print(totalPulses);
    Serial.print(F("; elapsed_ms=")); Serial.print((endUs - startedAtUs) / 1000.0F, 2);
    Serial.print(F("; timer_late=")); Serial.println(late);
    if (late) {
      stopTest();
      Serial.println(F("TIMING ERROR; test invalid; generator could not follow profile"));
      return;
    }
    if (movesDone == MOVE_COUNT) {
      session = false;
      Serial.println(F("DONE; 3 round trips; check physical return to reference; endpoint held"));
      return;
    }
    forward = !forward;
    // Each start is one beat apart when duration <=300ms. Always allow settling.
    const uint32_t actualMs = (endUs - startedAtUs) / 1000;
    const uint32_t pauseMs = actualMs + SETTLE_MS < BEAT_MS
        ? BEAT_MS - actualMs : SETTLE_MS;
    nextMoveAt = millis() + pauseMs;
  }
  if (!moving && static_cast<int32_t>(millis() - nextMoveAt) >= 0) beginMove();
}
