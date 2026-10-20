#include <Arduino.h>

// Uno: STEP D2, DIR D3, EN D4. Exactly 3200 pulses per completed run.
constexpr uint8_t STEP_PIN = 2, DIR_PIN = 3, EN_PIN = 4;
constexpr uint16_t TOTAL_PULSES = 3200;
constexpr uint32_t INTERVAL_US = 5000; // 200 pulses/s, about 16 seconds.
bool running = false;
uint16_t sent = 0;
uint32_t lastPulse = 0, heartbeat = 0, lastLog = 0;

void stopTest() {
  running = false;
  digitalWrite(STEP_PIN, LOW);
  digitalWrite(EN_PIN, HIGH);
  Serial.print(F("ABORTED; pulses=")); Serial.println(sent);
  Serial.println(F("STOPPED; repeat from a new mark; incomplete run is invalid"));
}

void setup() {
  digitalWrite(EN_PIN, HIGH);
  pinMode(EN_PIN, OUTPUT);
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  digitalWrite(STEP_PIN, LOW);
  digitalWrite(DIR_PIN, HIGH);
  Serial.begin(115200);
  Serial.println(F("CALIBRATION READY; s=3200 pulses at 200 pulses/s; x=stop; h=heartbeat"));
}

void loop() {
  while (Serial.available()) {
    const char command = Serial.read();
    if (command == 'x') stopTest();
    else if (command == 'h') heartbeat = millis();
    else if (command == 's' && !running) {
      sent = 0;
      digitalWrite(EN_PIN, LOW);
      lastPulse = micros();
      heartbeat = lastLog = millis();
      running = true;
      Serial.println(F("CALIBRATION START; count shaft revolutions; total=3200"));
    }
  }
  if (millis() - heartbeat >= 2000) {
    if (running) {
      stopTest();
      Serial.println(F("LINK TIMEOUT"));
    } else digitalWrite(EN_PIN, HIGH);
    return;
  }
  if (!running) return;
  if (micros() - lastPulse >= INTERVAL_US) {
    lastPulse = micros();
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(5);
    digitalWrite(STEP_PIN, LOW);
    ++sent;
    if (sent == TOTAL_PULSES) {
      running = false;
      // Keep holding the endpoint for reading the mark while connected.
      Serial.println(F("CALIBRATION DONE; pulses=3200; endpoint held; x=release"));
      return;
    }
  }
  if (millis() - lastLog >= 1000) {
    lastLog = millis();
    Serial.print(F("CALIBRATION pulses=")); Serial.print(sent);
    Serial.println(F("/3200"));
  }
}
