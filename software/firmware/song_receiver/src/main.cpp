#include <Arduino.h>

#include "config/machine_config.h"
#include "hardware/actuator_bank.h"
#include "hardware/arduino_output.h"
#include "playback/playback_controller.h"
#include "protocol/command_receiver.h"

namespace {

class SerialResponses : public guitar::firmware::ResponseOutput {
public:
    void line(const char* text) override { Serial.println(text); }
};

} // namespace

guitar::Receiver receiver;
guitar::firmware::MachineConfig machine = guitar::firmware::simulationConfig();
guitar::firmware::SimulatedOutput outputs;
guitar::firmware::ActuatorBank actuators(outputs, machine.stepper);
guitar::firmware::MotionPlanner planner(machine);
guitar::firmware::PlaybackController player(planner, actuators);
SerialResponses responses;
guitar::firmware::CommandReceiver commands(receiver, player, responses);

void setup() {
    actuators.begin();
    Serial.begin(115200);
    Serial.println(F("BOOT 1"));
}

void loop() {
    commands.tick(micros());
    // Bound Serial work so execution gets serviced even during a long upload.
    for (uint8_t bytes = 0; bytes < 32 && Serial.available(); ++bytes) {
        commands.accept(Serial.read(), micros());
        commands.tick(micros());
    }
}
