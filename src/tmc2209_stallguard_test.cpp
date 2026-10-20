#include <Arduino.h>
#include <AltSoftSerial.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <stdlib.h>

// Uno: STEP D2, DIR D3, EN D4, DIAG D5.
// AltSoftSerial: RX D8 -> PDN_UART; TX D9 -> 1k -> same PDN_UART.
// MS1=MS2=LOW -> UART address 0. Timer1 belongs to AltSoftSerial;
// Timer2 generates STEP. Do not combine with other timer-using tests.
constexpr uint8_t STEP_PIN = 2, DIR_PIN = 3, EN_PIN = 4, DIAG_PIN = 5;
constexpr uint16_t START_RATE = 1600; // 60 RPM at 1/8, not standstill sensing.
constexpr uint32_t ARM_DELAY_MS = 150, LINK_TIMEOUT_MS = 2000, MAX_RUN_MS = 5000;
AltSoftSerial driverSerial;
volatile bool moving = false, armed = false, diagTrip = false, reachedLimit = false;
volatile bool autoStop = true;
volatile uint16_t diagEvents = 0;
volatile uint16_t pulses = 0, pulseLimit = 5000;
bool active = false, warming = false;
uint16_t rate = START_RATE, targetRate = 3200;
uint8_t threshold = 20;
uint32_t heartbeat = 0, warmUntil = 0, startedAt = 0, lastRamp = 0;
uint32_t atSpeedAt = 0, lastSample = 0;
uint32_t lastDiagnostics = 0, sampleNumber = 0, lastSgAt = 0;
uint16_t lastSg = 0, sgMin = 65535, sgMax = 0;
uint8_t consecutiveLow = 0, chipDiag = 0, pwmScale = 0;
bool diagnosticsFresh = false;
char buffer[32];
uint8_t length = 0;
bool receiving = false;

// TMC2209 single-wire UART packets: address 0, big-endian registers, CRC-8.
// Explicit UART avoids SoftwareSerial's global pin-change ISR ownership.
uint8_t crc8(const uint8_t *data, uint8_t size) {
  uint8_t crc = 0;
  for (uint8_t i = 0; i < size; ++i) {
    uint8_t byte = data[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      const bool feedback = (crc >> 7) ^ (byte & 1);
      crc <<= 1;
      if (feedback) crc ^= 0x07;
      byte >>= 1;
    }
  }
  return crc;
}

uint8_t uartAddress = 0;
uint8_t lastRx[24], lastRxCount = 0;

void writeRegister(uint8_t reg, uint32_t value) {
  // The slave drives PDN_UART for four bit times after the reply stop bit.
  // At 19200 baud, 1ms also exceeds the 12-bit parser recovery interval.
  delayMicroseconds(1000);
  uint8_t packet[8] = {0x05, uartAddress, static_cast<uint8_t>(reg | 0x80),
      static_cast<uint8_t>(value >> 24), static_cast<uint8_t>(value >> 16),
      static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value), 0};
  packet[7] = crc8(packet, 7);
  driverSerial.write(packet, 8);
  driverSerial.flush();
  delayMicroseconds(200);
}

bool readRegister(uint8_t reg, uint32_t &value) {
  delayMicroseconds(1000); // Allow the previous slave reply to release the bus.
  while (driverSerial.available()) driverSerial.read();
  lastRxCount = 0;
  uint8_t request[4] = {0x05, uartAddress, reg, 0};
  request[3] = crc8(request, 3);
  driverSerial.write(request, 4);
  driverSerial.flush();
  uint8_t window[8], count = 0;
  const uint32_t began = millis();
  while (millis() - began < 30) {
    if (!driverSerial.available()) continue;
    const uint8_t byte = driverSerial.read();
    if (lastRxCount < sizeof(lastRx)) lastRx[lastRxCount++] = byte;
    if (count == 8) {
      for (uint8_t i = 0; i < 7; ++i) window[i] = window[i + 1];
      count = 7;
    }
    window[count++] = byte;
    // The shared TX/RX wire echoes our request; accept only a slave reply.
    if (count == 8 && window[0] == 0x05 && window[1] == 0xFF && window[2] == reg
        && crc8(window, 7) == window[7]) {
      value = static_cast<uint32_t>(window[3]) << 24 | static_cast<uint32_t>(window[4]) << 16
          | static_cast<uint32_t>(window[5]) << 8 | window[6];
      return true;
    }
  }
  return false;
}

// Can be called from either ISR. Disable the power stage immediately.
void haltFromInterrupt() {
  TCCR2B = 0;
  TIMSK2 = 0;
  PORTD &= ~_BV(PD2);
  PORTD |= _BV(PD4);
  moving = false;
}

ISR(PCINT2_vect) {
  if (moving && armed && (PIND & _BV(PD5))) {
    ++diagEvents;
    if (autoStop) {
      haltFromInterrupt();
      diagTrip = true;
    }
  }
}

ISR(TIMER2_COMPA_vect) {
  if (autoStop && armed && (PIND & _BV(PD5))) {
    haltFromInterrupt();
    diagTrip = true;
    return;
  }
  PORTD |= _BV(PD2);
  delayMicroseconds(2);
  PORTD &= ~_BV(PD2);
  if (++pulses >= pulseLimit) {
    haltFromInterrupt();
    reachedLimit = true;
  }
}

void setRate(uint16_t requested) {
  // Timer2: 16MHz/64 = 250kHz, period quantization 4us.
  const uint8_t top = static_cast<uint8_t>((250000UL + requested - 1) / requested - 1);
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    OCR2A = top;
    if (TCNT2 > top) TCNT2 = 0;
  }
}

void stopTest(const __FlashStringHelper *reason) {
  uint16_t count;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    haltFromInterrupt();
    armed = false;
    diagTrip = reachedLimit = false;
    count = pulses;
  }
  active = warming = false;
  Serial.print(reason);
  Serial.print(F("; pulses=")); Serial.print(count);
  Serial.print(F("; commanded_mm=")); Serial.println(count / 40.0F, 2);
  if (sampleNumber) {
    Serial.print(F("SUMMARY samples=")); Serial.print(sampleNumber);
    Serial.print(F("; last_sg=")); Serial.print(lastSg);
    Serial.print(F("; last_age_ms=")); Serial.print(millis() - lastSgAt);
    Serial.print(F("; cruise_min="));
    if (sgMin == 65535) Serial.print(F("NA")); else Serial.print(sgMin);
    Serial.print(F("; cruise_max=")); Serial.println(sgMax);
  }
  Serial.println(F("Position NOT measured; restore reference manually after stall"));
}

bool configureDriver() {
  uint32_t version, before, after, gconf, chopconf;
  if (!readRegister(0x06, version) || (version >> 24) != 0x21 || !readRegister(0x02, before)) {
    Serial.println(F("ERROR UART: no TMC2209 reply; check D8/D9, 1k, PDN and address0"));
    return false;
  }
  if (version & (1UL << 8)) {
    Serial.println(F("ERROR SPREAD pin high: select StealthChop on module before test"));
    return false;
  }
  // GCONF: analog VREF, external Rsense, StealthChop, UART config, STEP filter.
  writeRegister(0x00, 0x000001C1UL);
  // CHOPCONF: intpol=1, MRES=5 (1/8), vsense=0, TBL=1, TOFF=4.
  writeRegister(0x6C, 0x15008004UL);
  writeRegister(0x70, 0xC10D0024UL); // PWM autoscale/autograd enabled.
  writeRegister(0x10, 0x00081F10UL); // IHOLD=16, IRUN=31, IHOLDDELAY=8.
  writeRegister(0x11, 20);         // Standstill power-down delay.
  writeRegister(0x13, 0);          // TPWMTHRS: do not switch to SpreadCycle.
  writeRegister(0x42, 0);          // CoolStep disabled, fixed current.
  writeRegister(0x40, threshold);  // SGTHRS: compare SG_RESULT <= 2*SGTHRS.
  writeRegister(0x14, 0);          // TCOOLTHRS: DIAG stall disabled until armed.
  writeRegister(0x01, 7);          // Clear GSTAT latched reset/errors.
  if (!readRegister(0x02, after) || static_cast<uint8_t>(after - before) != 10) {
    Serial.println(F("ERROR UART: configuration writes not acknowledged"));
    return false;
  }
  if (!readRegister(0x00, gconf) || gconf != 0x000001C1UL
      || !readRegister(0x6C, chopconf) || chopconf != 0x15008004UL) {
    Serial.println(F("ERROR UART: configuration readback mismatch"));
    return false;
  }
  Serial.println(F("UART OK; StealthChop; microstep=8; analog VREF; IRUN=31; CoolStep=off"));
  return true;
}

void diagnoseUart() {
  Serial.print(F("UART_SCAN; RX_D8_idle=")); Serial.println(digitalRead(8));
  bool foundZero = false, foundAny = false;
  for (uint8_t address = 0; address < 4; ++address) {
    uartAddress = address;
    uint32_t ioin = 0;
    const bool reply = readRegister(0x06, ioin);
    Serial.print(F("UART address=")); Serial.print(address);
    Serial.print(F("; received_bytes=")); Serial.print(lastRxCount);
    Serial.print(F("; raw_hex="));
    for (uint8_t i = 0; i < lastRxCount; ++i) {
      if (lastRx[i] < 16) Serial.print('0');
      Serial.print(lastRx[i], HEX); Serial.print(' ');
    }
    if (reply) {
      Serial.print(F("; VALID_REPLY version=0x")); Serial.print(ioin >> 24, HEX);
      if ((ioin >> 24) == 0x21) {
        foundAny = true;
        if (address == 0) foundZero = true;
      }
    } else if (lastRxCount == 0) Serial.print(F("; NO_RX: check D8 and common ground"));
    else if (lastRxCount == 4) Serial.print(F("; POSSIBLE_ECHO_ONLY: check VM/VIO, PDN, DIP"));
    else Serial.print(F("; INVALID_REPLY: check wiring, levels and UART module jumper"));
    Serial.println();
  }
  uartAddress = 0;
  if (foundZero) configureDriver();
  else if (foundAny) Serial.println(F("ERROR UART: chip found at nonzero address; set MS1/MS2 OFF then retry"));
  else Serial.println(F("ERROR UART: no valid reply at any address; send full UART_SCAN log"));
}

void startTest(uint16_t rpm, uint16_t sensitivity, uint16_t distance, bool forward, bool stopOnLoad) {
  if (active) { Serial.println(F("ERROR busy")); return; }
  if (rpm < 60 || rpm > 240 || sensitivity > 255 || distance < 1 || distance > 125) {
    Serial.println(F("ERROR range: rpm60..240, SGTHRS0..255, mm1..125, dir0/1"));
    return;
  }
  threshold = sensitivity;
  if (!configureDriver()) return;
  targetRate = static_cast<uint32_t>(rpm) * 1600UL / 60UL;
  rate = START_RATE;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    pulses = 0;
    pulseLimit = distance * 40;
    diagTrip = reachedLimit = armed = false;
    autoStop = stopOnLoad;
    diagEvents = 0;
  }
  sampleNumber = 0;
  sgMin = 65535;
  sgMax = lastSg = consecutiveLow = 0;
  diagnosticsFresh = false;
  digitalWrite(DIR_PIN, forward ? HIGH : LOW);
  digitalWrite(EN_PIN, LOW);
  heartbeat = millis();
  warmUntil = heartbeat + 250; // Standstill time for StealthChop auto-tuning.
  active = warming = true;
  atSpeedAt = 0;
  Serial.print(F("START; rpm=")); Serial.print(rpm);
  Serial.print(F("; SGTHRS=")); Serial.print(threshold);
  Serial.print(F("; travel_limit_mm=")); Serial.println(distance);
  Serial.println(autoStop ? F("MODE=AUTO_STOP; DIAG + 2 consecutive low SG readings")
                         : F("MODE=OBSERVE; load/DIAG will NOT stop motor; limited travel and 5s timeout remain"));
  Serial.println(F("WARMUP then ramp; detection INACTIVE until ARMED"));
}

void parseCommand() {
  buffer[length] = '\0';
  unsigned long values[4];
  char *cursor = buffer;
  for (uint8_t i = 0; i < 4; ++i) {
    char *end;
    values[i] = strtoul(cursor, &end, 10);
    if (end == cursor || (i < 3 ? *end != ',' : *end != '\0')) {
      Serial.println(F("ERROR command format")); return;
    }
    cursor = end + 1;
  }
  if (values[0] < 60 || values[0] > 240 || values[1] > 255 || values[2] < 1 || values[2] > 125 || values[3] > 1) {
    Serial.println(F("ERROR command range")); return;
  }
  startTest(values[0], values[1], values[2], values[3], autoStop);
}

void setup() {
  digitalWrite(EN_PIN, HIGH);
  pinMode(EN_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(DIAG_PIN, INPUT); // External 10k pulldown recommended for DIAG wire test.
  TCCR2A = _BV(WGM21);
  TCCR2B = TIMSK2 = 0;
  PCMSK2 |= _BV(PCINT21); // D5 only; D2 is STEP and D3 is DIR.
  PCIFR = _BV(PCIF2);
  PCICR |= _BV(PCIE2);
  Serial.begin(115200);
  driverSerial.begin(19200);
  Serial.println(F("SG READY V2; STEP=D2 DIR=D3 EN=D4 DIAG=D5 RX=D8 TX=D9"));
  Serial.println(F("S...=auto stop; O...=observe; ?=UART; m=load marker; x=stop; h=heartbeat"));
}

void loop() {
  if (diagTrip) { stopTest(F("DIAG_STOP: stall OR driver fault")); return; }
  if (reachedLimit) {
    const bool wasArmed = armed;
    stopTest(wasArmed ? F("DONE_LIMIT: no DIAG detected") : F("DONE_UNARMED: move too short for detection"));
    return;
  }
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == 'h') heartbeat = millis();
    else if (c == 'x') { receiving = false; stopTest(F("STOPPED")); }
    else if (c == '?' && !active) { diagnoseUart(); }
    else if ((c == 'S' || c == 'O') && !active) {
      autoStop = c == 'S'; receiving = true; length = 0;
    }
    else if (c == 'm' && active) {
      Serial.print(F("LOAD_MARK t_ms=")); Serial.print(millis() - startedAt);
      Serial.print(F("; last_sg=")); Serial.println(lastSg);
    }
    else if (receiving && c == '\n') { receiving = false; parseCommand(); }
    else if (receiving && c != '\r') {
      if (length < sizeof(buffer) - 1 && ((c >= '0' && c <= '9') || c == ',')) buffer[length++] = c;
      else { receiving = false; Serial.println(F("ERROR command")); }
    }
  }
  if (!active) return;
  const uint32_t now = millis();
  if (now - heartbeat >= LINK_TIMEOUT_MS) { stopTest(F("LINK_TIMEOUT")); return; }
  if (warming) {
    if (static_cast<int32_t>(now - warmUntil) < 0) return;
    warming = false;
    startedAt = lastRamp = lastSample = lastDiagnostics = now;
    setRate(rate);
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
      TCNT2 = 0;
      TIFR2 = _BV(OCF2A);
      moving = true;
      TIMSK2 = _BV(OCIE2A);
      TCCR2B = _BV(CS22); // /64
    }
  }
  if (now - startedAt >= MAX_RUN_MS) { stopTest(F("TIME_LIMIT")); return; }
  if (now - lastRamp >= 20 && rate < targetRate) {
    lastRamp = now;
    rate = min(static_cast<uint16_t>(rate + 80), targetRate); // 4000 pulses/s^2.
    setRate(rate);
  }
  if (rate == targetRate && !atSpeedAt) atSpeedAt = now;
  if (!armed && atSpeedAt && now - atSpeedAt >= ARM_DELAY_MS && moving) {
    uint32_t before, after;
    if (!readRegister(0x02, before)) { stopTest(F("UART_LOST")); return; }
    writeRegister(0x14, 0xFFFFF);
    if (!readRegister(0x02, after) || static_cast<uint8_t>(after - before) != 1) {
      stopTest(F("UART_LOST: cannot enable DIAG")); return;
    }
    if (!moving) return;
    armed = true;
    Serial.println(autoStop ? F("ARMED: DIAG + software SG detection enabled")
                           : F("ARMED: OBSERVATION ONLY; load does NOT stop motor"));
    if (autoStop && digitalRead(DIAG_PIN)) {
      stopTest(F("DIAG_STOP: high at arming; check threshold/fault/wiring")); return;
    }
  }
  if (now - lastSample >= 50 && moving) {
    lastSample = now;
    uint32_t sg;
    if (!readRegister(0x41, sg)) {
      stopTest(F("UART_LOST")); return;
    }
    if (!moving) return; // A DIAG interrupt can have stopped us during UART read.
    lastSg = sg & 0x3FF;
    lastSgAt = millis();
    ++sampleNumber;
    if (armed) {
      sgMin = min(sgMin, lastSg);
      sgMax = max(sgMax, lastSg);
    }
    if (now - lastDiagnostics >= 200) {
      uint32_t ioin, status, pwm;
      lastDiagnostics = now;
      if (!readRegister(0x06, ioin) || (ioin >> 24) != 0x21
          || !readRegister(0x6F, status) || !readRegister(0x71, pwm)) {
        stopTest(F("UART_LOST")); return;
      }
      chipDiag = (ioin >> 4) & 1;
      pwmScale = pwm & 0xFF;
      diagnosticsFresh = true;
      if (!moving) return;
      if (status & 0x3FUL) { stopTest(F("DRIVER_FAULT: temperature/short warning")); return; }
      if (!(status & (1UL << 30))) { stopTest(F("MODE_ERROR: StealthChop not active")); return; }
    }
    uint16_t count;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { count = pulses; }
    Serial.print(F("SG_RESULT=")); Serial.print(lastSg);
    Serial.print(F("; trip_below=")); Serial.print(static_cast<uint16_t>(threshold) * 2);
    Serial.print(F("; rpm_cmd=")); Serial.print(250000.0F / (OCR2A + 1) * 60 / 1600, 1);
    Serial.print(F("; armed=")); Serial.print(armed);
    Serial.print(F("; pulses=")); Serial.print(count);
    Serial.print(F("; n=")); Serial.print(sampleNumber);
    Serial.print(F("; t_ms=")); Serial.print(lastSgAt - startedAt);
    Serial.print(F("; DIAG_D5=")); Serial.print(digitalRead(DIAG_PIN));
    Serial.print(F("; chip_DIAG=")); Serial.print(chipDiag);
    Serial.print(F("; PWM=")); Serial.println(pwmScale);
    if (diagnosticsFresh) {
      diagnosticsFresh = false;
      if (pwmScale >= 250) Serial.println(F("WARN PWM_NEAR_LIMIT: current may be voltage-limited; SG response may be unreliable"));
      if (armed && chipDiag && !digitalRead(DIAG_PIN))
        Serial.println(F("WARN DIAG_MISMATCH: chip pin was high but D5 is low; check DIAG wiring (samples not simultaneous)"));
    }
    if (autoStop && armed) {
      if (lastSg <= static_cast<uint16_t>(threshold) * 2) ++consecutiveLow;
      else consecutiveLow = 0;
      if (consecutiveLow >= 2) {
        stopTest(F("SG_SOFTWARE_STOP: 2 low readings; DIAG interrupt not seen"));
        return;
      }
    }
  }
}
