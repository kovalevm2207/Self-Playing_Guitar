#include <Arduino.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <math.h>
#include <stdlib.h>

// GT2 20T = 40 mm/rev; 1/8 = 1600 STEP/rev = 40 STEP/mm.
// No homing or end switches: establish clearance/reference manually first.
constexpr uint8_t STEP_PIN = 2, DIR_PIN = 3, EN_PIN = 4;
uint8_t moveCount = 6; // Accuracy mode can select up to 30 round trips.
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
// Only the main loop grants/consumes this permission. ISR completion alone
// must never launch another move before its result has been processed.
bool moveScheduled = false;
#ifdef ACCURACY_TEST
bool referenceReady = false, awaitingMeasurement = false;
uint32_t measurementBegan = 0;
#endif
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
    segmentPulses[segmentCount] = count;
    // Longer accuracy moves may require /256 instead of /8 or /64.
    const bool verySlow = ticks > 65535.0F * 8;
    const bool slow = ticks > 65535.0F;
    segmentTicks[segmentCount] = static_cast<uint16_t>(ticks / (verySlow ? 32 : slow ? 8 : 1) + 0.5F);
    segmentClock[segmentCount] = verySlow ? _BV(CS12)
        : slow ? _BV(CS11) | _BV(CS10) : _BV(CS11);
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
  moveScheduled = false;
#ifdef ACCURACY_TEST
  referenceReady = awaitingMeasurement = false;
#endif
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

void startSession(uint16_t distance, uint16_t duration, uint8_t cycles = 3) {
  if (session) { Serial.println(F("ERROR busy")); return; }
#ifdef ACCURACY_TEST
  if (!referenceReady) { Serial.println(F("ERROR set reference first with z")); return; }
  if (distance < 1 || distance > 300 || duration < 300 || duration > 5000 || cycles < 1 || cycles > 30) {
    Serial.println(F("ERROR accuracy range: mm1..300, ms300..5000, cycles1..30")); return;
  }
  awaitingMeasurement = false;
#else
  if (distance < 1 || distance > 300 || duration < 20 || duration > 5000) {
    Serial.println(F("ERROR range: distance=1..300 mm; duration=20..5000 ms"));
    return;
  }
  // Trapezoid peak: distance*40 / (0.7*duration/1000). Bound ISR demand.
  if (static_cast<uint32_t>(distance) * 40000UL > static_cast<uint32_t>(duration) * 35000UL) {
    Serial.println(F("ERROR peak exceeds 50000 STEP/s; increase duration or reduce distance"));
    return;
  }
#endif
  moveCount = cycles * 2;
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
  Serial.print(F("; moves=")); Serial.print(moveCount);
  Serial.print(F("; move_ms=")); Serial.print(durationMs);
  Serial.print(F("; peak_rpm=")); Serial.print(peakMmS * 60.0F / 40.0F, 1);
  Serial.print(F("; accel_mm_s2=")); Serial.println(peakMmS / (0.3F * seconds), 0);
  Serial.println(F("These are commanded values, NOT measured shaft motion"));
  session = true;
  // First move begins after the main loop has drained serial input.
  nextMoveAt = millis() + 20;
  moveScheduled = true;
}

void setup() {
  digitalWrite(EN_PIN, HIGH);
  pinMode(EN_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  TCCR1A = TCCR1B = TIMSK1 = 0;
  Serial.begin(115200);
#ifdef ACCURACY_TEST
  Serial.println(F("ACCURACY READY V2; STEP=D2 DIR=D3 EN=D4; 40 STEP/mm; no homing/encoder"));
  Serial.println(F("z=energize/reference; Adistance,duration,cycles newline; c=measurement recorded; x=stop; h=heartbeat"));
#else
  Serial.println(F("FRET READY V3; STEP=D2 DIR=D3 EN=D4; GT2 20T; microstep=8; ms20..5000; peak_max=50000"));
  Serial.println(F("Mdistance,duration followed by newline; x=stop; h=heartbeat"));
#endif
}

void loop() {
  while (Serial.available()) {
    const char command = Serial.read();
    if (command == 'h') heartbeat = millis();
    else if (command == 'x') { receivingCommand = false; stopTest(); }
#ifdef ACCURACY_TEST
    else if (command == 'z' && !session) {
      digitalWrite(EN_PIN, LOW);
      delay(100); // Let the energized rotor settle before the user zeros the gauge.
      referenceReady = true;
      heartbeat = millis();
      Serial.println(F("REFERENCE_READY; driver holding; set physical gauge zero now; no homing performed"));
    }
    else if (command == 'c' && session && awaitingMeasurement) {
      awaitingMeasurement = false;
      if (movesDone == moveCount) {
        session = false;
        Serial.println(F("DONE_ACCURACY; all measurements acknowledged; endpoint held"));
      } else {
        forward = !forward;
        nextMoveAt = millis() + 250;
        moveScheduled = true;
      }
    }
    else if (command == 'A') { receivingCommand = true; commandLength = 0; }
#else
    else if (command == 'M') { receivingCommand = true; commandLength = 0; }
#endif
    else if (receivingCommand && command == '\n') {
      receivingCommand = false;
      commandBuffer[commandLength] = '\0';
      char *separator;
      const unsigned long distance = strtoul(commandBuffer, &separator, 10);
      if (separator == commandBuffer || *separator != ',') { Serial.println(F("ERROR format")); continue; }
      char *end;
      const unsigned long duration = strtoul(separator + 1, &end, 10);
#ifdef ACCURACY_TEST
      if (end == separator + 1 || *end != ',') { Serial.println(F("ERROR accuracy format")); continue; }
      char *cyclesEnd;
      const unsigned long cycles = strtoul(end + 1, &cyclesEnd, 10);
      if (cyclesEnd == end + 1 || *cyclesEnd || distance < 1 || distance > 300
          || duration < 300 || duration > 5000 || cycles < 1 || cycles > 30) {
        Serial.println(F("ERROR accuracy command range")); continue;
      }
      startSession(distance, duration, cycles);
#else
      if (end == separator + 1 || *end || distance < 1 || distance > 300 || duration < 20 || duration > 5000) {
        Serial.println(F("ERROR invalid distance/duration"));
        continue;
      }
      startSession(distance, duration);
#endif
    } else if (receivingCommand && command != '\r') {
      if (commandLength < sizeof(commandBuffer) - 1 && ((command >= '0' && command <= '9') || command == ','))
        commandBuffer[commandLength++] = command;
      else { receivingCommand = false; Serial.println(F("ERROR invalid command")); }
    }
  }
  if (millis() - heartbeat >= 2000) {
    if (session) { stopTest(); Serial.println(F("LINK TIMEOUT")); }
    else {
      digitalWrite(EN_PIN, HIGH);
#ifdef ACCURACY_TEST
      if (referenceReady) Serial.println(F("REFERENCE_LOST; link timeout; establish zero again"));
      referenceReady = false;
#endif
    }
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
    Serial.print('/'); Serial.print(moveCount);
    Serial.print(F("; direction=")); Serial.print(forward ? F("OUT") : F("BACK"));
    Serial.print(F("; pulses=")); Serial.print(totalPulses);
    Serial.print(F("; elapsed_ms=")); Serial.print((endUs - startedAtUs) / 1000.0F, 2);
    Serial.print(F("; timer_late=")); Serial.println(late);
    if (late) {
      stopTest();
      Serial.println(F("TIMING ERROR; test invalid; generator could not follow profile"));
      return;
    }
#ifdef ACCURACY_TEST
    awaitingMeasurement = true;
    measurementBegan = millis();
    Serial.print(F("MEASURE index=")); Serial.print(movesDone);
    Serial.print(F("; cycle=")); Serial.print((movesDone + 1) / 2);
    Serial.print(F("; side=")); Serial.print(forward ? F("OUT") : F("BACK"));
    Serial.print(F("; target_mm=")); Serial.print(forward ? distanceMm : 0);
    Serial.print(F("; pulses=")); Serial.print(totalPulses);
    Serial.print(F("; elapsed_us=")); Serial.println(endUs - startedAtUs);
    return;
#else
    if (movesDone == moveCount) {
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
    moveScheduled = true;
#endif
  }
#ifdef ACCURACY_TEST
  if (awaitingMeasurement) {
    if (millis() - measurementBegan > 120000UL) {
      stopTest(); Serial.println(F("MEASUREMENT_TIMEOUT; partial test invalid"));
    }
    return;
  }
#endif
  if (moveScheduled && !moving && static_cast<int32_t>(millis() - nextMoveAt) >= 0) {
    moveScheduled = false;
    beginMove();
  }
}
