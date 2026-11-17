#include <Arduino.h>

// Arduino Uno -> TMC2209: D2 -> STEP, D3 -> DIR, D4 -> EN/ENN.
// VIO -> 5V; common GND; VM -> external 12V supply.
// Set the driver's current limit before powering the motor.
// Never connect/disconnect the motor while power is on.
// MS1 and MS2 -> GND. UART is not required.
constexpr uint8_t STEP_PIN = 2;
constexpr uint8_t DIR_PIN = 3;
constexpr uint8_t ENABLE_PIN = 4;
constexpr unsigned int PULSES_PER_MOVE = 400;

void moveMotor(bool direction) {
  digitalWrite(DIR_PIN, direction ? HIGH : LOW);
  delayMicroseconds(20);

  for (unsigned int i = 0; i < PULSES_PER_MOVE; ++i) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(STEP_PIN, LOW);
    delay(5);  // Slow start for an unloaded motor, about 200 pulses/s.
  }
}

void setup() {
  digitalWrite(ENABLE_PIN, HIGH);
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(STEP_PIN, LOW);
  pinMode(STEP_PIN, OUTPUT);
  digitalWrite(DIR_PIN, LOW);
  pinMode(DIR_PIN, OUTPUT);

  Serial.begin(9600);
  Serial.println(F("TMC2209 connection test: STEP=D2 DIR=D3 EN=D4"));
  Serial.println(F("Starting in 3 seconds. Test with no mechanical load."));
  delay(3000);
  digitalWrite(ENABLE_PIN, LOW);
}

void loop() {
  Serial.println(F("Forward: 400 STEP pulses"));
  moveMotor(true);
  delay(1000);

  Serial.println(F("Reverse: 400 STEP pulses"));
  moveMotor(false);
  delay(1000);
}
