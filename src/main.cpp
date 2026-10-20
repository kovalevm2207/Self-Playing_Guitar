#include <Arduino.h>
#include <AccelStepper.h>

constexpr uint8_t STEP_PIN = 2;
constexpr uint8_t ENABLE_PIN = 13;
constexpr uint8_t DIR_PIN = 8;

constexpr float TRAVEL_MM = 100.0F;
constexpr int MOTOR_FULL_STEPS_PER_REVOLUTION = 200;
constexpr int MICROSTEPS = 1;
constexpr float STEPS_PER_MOTOR_REVOLUTION =
    MOTOR_FULL_STEPS_PER_REVOLUTION * MICROSTEPS;

// Двигатель 1,8 градуса, полный шаг, ремень GT2, шкив 20 зубьев:
// 200 шагов / (2 мм * 20 зубьев) = 5 шагов/мм.
// Для другой механики это значение нужно откалибровать.
constexpr float STEPS_PER_MM = 5.0F;

constexpr float MAX_SPEED_STEPS_PER_SECOND = 400.0F;
constexpr float ACCELERATION_STEPS_PER_SECOND_SQUARED = 250.0F;
constexpr unsigned long START_DELAY_MS = 3000;
constexpr unsigned long LOG_INTERVAL_MS = 250;

constexpr long TRAVEL_STEPS = static_cast<long>(TRAVEL_MM * STEPS_PER_MM);

AccelStepper motor(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

enum class MotionState {
  WaitingToStart,
  MovingUp,
  MovingDown
};

MotionState state = MotionState::WaitingToStart;
unsigned long stateStartedAt = 0;
unsigned long lastLogAt = 0;

void enterState(MotionState newState) {
  state = newState;
  stateStartedAt = millis();
}

void printMotionLog() {
  if (millis() - lastLogAt < LOG_INTERVAL_MS) {
    return;
  }
  lastLogAt = millis();

  const float speedStepsPerSecond = motor.speed();
  const float speedRpm =
      speedStepsPerSecond * 60.0F / STEPS_PER_MOTOR_REVOLUTION;
  const float positionMm = motor.currentPosition() / STEPS_PER_MM;

  Serial.print("position_mm=");
  Serial.print(positionMm, 1);
  Serial.print("; speed_steps_s=");
  Serial.print(speedStepsPerSecond, 1);
  Serial.print("; speed_rpm=");
  Serial.print(speedRpm, 1);
  Serial.print("; direction=");

  if (speedStepsPerSecond > 0.0F) {
    Serial.println("UP");
  } else if (speedStepsPerSecond < 0.0F) {
    Serial.println("DOWN");
  } else {
    Serial.println("STOP");
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(ENABLE_PIN, OUTPUT);

  // У A4988/DRV8825 вход ENABLE активен низким уровнем.
  digitalWrite(ENABLE_PIN, LOW);

  motor.setMaxSpeed(MAX_SPEED_STEPS_PER_SECOND);
  motor.setAcceleration(ACCELERATION_STEPS_PER_SECOND_SQUARED);
  motor.setCurrentPosition(0);

  stateStartedAt = millis();
  Serial.print("Carriage travel, mm: ");
  Serial.println(TRAVEL_MM, 1);
  Serial.print("Configured maximum speed, rpm: ");
  Serial.println(MAX_SPEED_STEPS_PER_SECOND * 60.0F /
                     STEPS_PER_MOTOR_REVOLUTION,
                 1);
  Serial.println("Starting in 3 seconds...");
}

void loop() {
  // run() нужно вызывать постоянно, иначе мотор остановится.
  motor.run();
  printMotionLog();

  switch (state) {
    case MotionState::WaitingToStart:
      if (millis() - stateStartedAt >= START_DELAY_MS) {
        Serial.println("Moving up");
        motor.moveTo(TRAVEL_STEPS);
        enterState(MotionState::MovingUp);
      }
      break;

    case MotionState::MovingUp:
      if (motor.distanceToGo() == 0) {
        Serial.println("Top reached; reversing down");
        motor.moveTo(0);
        enterState(MotionState::MovingDown);
      }
      break;

    case MotionState::MovingDown:
      if (motor.distanceToGo() == 0) {
        Serial.println("Bottom reached; reversing up");
        motor.moveTo(TRAVEL_STEPS);
        enterState(MotionState::MovingUp);
      }
      break;
  }
}
