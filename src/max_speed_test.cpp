#include <Arduino.h>
#include <AccelStepper.h>

constexpr uint8_t STEP_PIN = 2;
constexpr uint8_t ENABLE_PIN = 13;
constexpr uint8_t DIR_PIN = 8;

constexpr float TRAVEL_MM = 100.0F;
constexpr float STEPS_PER_MM = 5.0F;
constexpr float STEPS_PER_REVOLUTION = 200.0F;
constexpr long TRAVEL_STEPS = static_cast<long>(TRAVEL_MM * STEPS_PER_MM);

// Тест начинает с уже проверенной скорости и повышает её после каждого цикла.
constexpr float START_SPEED_STEPS_PER_SECOND = 400.0F;
constexpr float SPEED_INCREMENT_STEPS_PER_SECOND = 200.0F;
constexpr float MAX_TEST_SPEED_STEPS_PER_SECOND = 2000.0F;
constexpr float ACCELERATION_STEPS_PER_SECOND_SQUARED = 1500.0F;

constexpr unsigned long START_DELAY_MS = 3000;
constexpr unsigned long LOG_INTERVAL_MS = 200;

AccelStepper motor(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

float testSpeed = START_SPEED_STEPS_PER_SECOND;
bool movingUp = true;
unsigned long lastLogAt = 0;

float stepsPerSecondToRpm(float stepsPerSecond) {
  return stepsPerSecond * 60.0F / STEPS_PER_REVOLUTION;
}

void printStatus() {
  if (millis() - lastLogAt < LOG_INTERVAL_MS) {
    return;
  }
  lastLogAt = millis();

  Serial.print("position_mm=");
  Serial.print(motor.currentPosition() / STEPS_PER_MM, 1);
  Serial.print("; actual_speed_steps_s=");
  Serial.print(motor.speed(), 1);
  Serial.print("; actual_speed_rpm=");
  Serial.print(stepsPerSecondToRpm(motor.speed()), 1);
  Serial.print("; configured_limit_rpm=");
  Serial.println(stepsPerSecondToRpm(testSpeed), 1);
}

void setup() {
  Serial.begin(9600);
  pinMode(ENABLE_PIN, OUTPUT);
  digitalWrite(ENABLE_PIN, LOW);

  motor.setAcceleration(ACCELERATION_STEPS_PER_SECOND_SQUARED);
  motor.setMaxSpeed(testSpeed);
  motor.setCurrentPosition(0);

  Serial.println("WARNING: maximum-speed carriage test");
  Serial.println("Speed increases after every complete up/down cycle");
  Serial.print("Initial limit, rpm: ");
  Serial.println(stepsPerSecondToRpm(testSpeed), 1);
  Serial.print("Final limit, rpm: ");
  Serial.println(stepsPerSecondToRpm(MAX_TEST_SPEED_STEPS_PER_SECOND), 1);
  Serial.println("Starting in 3 seconds...");

  delay(START_DELAY_MS);
  motor.moveTo(TRAVEL_STEPS);
}

void loop() {
  motor.run();
  printStatus();

  if (motor.distanceToGo() != 0) {
    return;
  }

  if (movingUp) {
    movingUp = false;
    motor.moveTo(0);
    Serial.println("TOP: reversing down");
    return;
  }

  movingUp = true;

  if (testSpeed < MAX_TEST_SPEED_STEPS_PER_SECOND) {
    testSpeed += SPEED_INCREMENT_STEPS_PER_SECOND;
    if (testSpeed > MAX_TEST_SPEED_STEPS_PER_SECOND) {
      testSpeed = MAX_TEST_SPEED_STEPS_PER_SECOND;
    }
    motor.setMaxSpeed(testSpeed);
    Serial.print("NEW LIMIT: steps/s=");
    Serial.print(testSpeed, 1);
    Serial.print("; rpm=");
    Serial.println(stepsPerSecondToRpm(testSpeed), 1);
  } else {
    Serial.println("Maximum configured test speed reached");
  }

  motor.moveTo(TRAVEL_STEPS);
}
