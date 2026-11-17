#undef NDEBUG
#include <cassert>
#include <iostream>
#include <vector>

#include "config/machine_config.h"
#include "hardware/actuator_bank.h"
#include "playback/playback_controller.h"

using namespace guitar;
using namespace guitar::firmware;

struct Edge {
    uint32_t time;
    uint8_t pin;
    bool high;
};

struct ProbeOutput : DigitalOutput {
    uint32_t now = 0;
    bool levels[32]{};
    std::vector<Edge> edges;
    void configure(uint8_t) override {}
    void write(uint8_t pin, bool high) override {
        if (levels[pin] != high) {
            levels[pin] = high;
            edges.push_back({now, pin, high});
        }
    }
    void assertReleased() const {
        for (uint8_t pin = 0; pin < 24; ++pin) {
            if (pin % 4 != 1) { // DIR may retain its last level.
                assert(!levels[pin]);
            }
        }
    }
};

void checkDrivers() {
    ProbeOutput output;
    StepperDriver stepper(output, 0, 1, {1000, 20});
    stepper.begin();
    stepper.moveTo(2, 0);
    for (uint32_t now = 0; now <= 2020; now += 10) {
        output.now = now;
        stepper.tick(now);
    }
    assert(stepper.position() == 2 && !stepper.moving());
    assert(output.edges.size() == 5);
    assert(output.edges[1].time == 1000 && output.edges[1].high);
    assert(output.edges[2].time == 1020 && !output.edges[2].high);
    assert(output.edges[3].time == 2000 && output.edges[3].high);
    assert(output.edges[4].time == 2020 && !output.edges[4].high);
    stepper.moveTo(-1, 3000);
    output.now = 4000;
    stepper.tick(4000);
    assert(stepper.position() == 1 && !output.levels[1]);
    stepper.stop();
    assert(!output.levels[0] && !stepper.moving());
    const auto edges = output.edges.size();
    stepper.tick(100000);
    assert(output.edges.size() == edges);

    SolenoidDriver solenoid(output, 2);
    solenoid.begin();
    solenoid.activate(0xfffffff0U, 40);
    solenoid.tick(0x17);
    assert(solenoid.active());
    solenoid.tick(0x18);
    assert(!solenoid.active());
    solenoid.activate(0, 12000000);
    assert(!solenoid.active());
}

void checkPlanner() {
    auto config = simulationConfig(0x3f);
    MotionPlanner planner(config);
    int16_t positions[6]{};
    const Note impossible[]{{1, 0, 50}, {1, 24, 50}};
    assert(!planner.validate(impossible, 2, positions));
    const Note possible[]{{1, 3, 100}, {1, 0, 100}, {2, 1, 100}};
    assert(planner.validate(possible, 3, positions));
    NotePlan plan{};
    assert(planner.plan({1, 0, 100}, 20, plan));
    assert(plan.targetSteps == 20 && plan.moveUs == 0);
    assert(!planner.plan({0, 0, 100}, 0, plan));
    config.stringMask = 1;
    assert(!planner.validate(possible, 3, positions));
    config.stepper.pulseUs = 1000;
    assert(!planner.plan({1, 1, 100}, 0, plan));
}

void checkSequence(uint32_t start) {
    const auto config = simulationConfig(0x3f);
    MotionPlanner planner(config);
    ProbeOutput output;
    ActuatorBank actuators(output, config.stepper);
    actuators.begin();
    PlaybackController player(planner, actuators);
    const Note notes[]{{1, 3, 100}, {1, 0, 120}, {2, 1, 80}};
    assert(player.play(notes, 3, start));
    assert(!player.play(notes, 3, start));
    for (uint32_t elapsed = 0; elapsed <= 310000; elapsed += 10) {
        output.now = start + elapsed;
        player.tick(output.now);
        // Neither carriage may move while any string is clamped.
        if (output.levels[0] || output.levels[4]) {
            for (uint8_t string = 0; string < 6; ++string) {
                assert(!output.levels[string * 4 + 2]);
            }
        }
    }
    assert(player.phase() == PlaybackPhase::Completed);
    output.assertReleased();
    std::vector<uint32_t> picks;
    for (const auto& edge : output.edges) {
        if (edge.pin % 4 == 3 && edge.high) {
            picks.push_back(edge.time - start);
        }
    }
    assert((picks == std::vector<uint32_t>{9020, 109020, 229020}));
    assert(actuators.axis(1).position() == 6);
    assert(actuators.axis(2).position() == 2);
}

void checkStopAndLateTick() {
    // Stop in each preparation stage and while sounding.
    for (const uint32_t stopAt : {0U, 2000U, 7500U, 8500U, 10000U}) {
        const auto config = simulationConfig();
        MotionPlanner planner(config);
        ProbeOutput output;
        ActuatorBank actuators(output, config.stepper);
        actuators.begin();
        PlaybackController player(planner, actuators);
        const Note note{1, 3, 100};
        assert(player.play(&note, 1, 0));
        for (uint32_t now = 0; now <= stopAt; now += 10) {
            output.now = now;
            player.tick(now);
        }
        player.stop();
        assert(!player.busy());
        output.assertReleased();
        const auto count = output.edges.size();
        player.tick(500000);
        assert(output.edges.size() == count);
    }

    const auto config = simulationConfig();
    MotionPlanner planner(config);
    ProbeOutput output;
    ActuatorBank actuators(output, config.stepper);
    actuators.begin();
    PlaybackController player(planner, actuators);
    const Note note{1, 3, 100};
    assert(player.play(&note, 1, 0));
    player.tick(100000);
    assert(player.phase() == PlaybackPhase::Fault);
    output.assertReleased();
    assert(output.edges.empty()); // No late catch-up pulse after a stalled loop.
    const Note impossible[]{{1, 0, 50}, {1, 24, 50}};
    assert(!player.play(impossible, 2, 200000));
    assert(output.edges.empty());
}

int main() {
    checkDrivers();
    checkPlanner();
    checkSequence(0);
    checkSequence(0xfffff000U);
    checkStopAndLateTick();
    std::cout << "PASS: STEP/DIR edges, solenoid timing, planning, onset intervals, "
                 "open strings, stop, deadline fault, clock wrap\n";
}
