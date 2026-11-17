#undef NDEBUG
#include <cassert>
#include <iostream>

#include "application/transfer_session.h"
#include "protocol/command_receiver.h"

using namespace guitar;

// The real application session and firmware parser communicate over queued lines.
struct VirtualDevice : DeviceTransport, firmware::ResponseOutput {
    firmware::MachineConfig config = firmware::simulationConfig();
    firmware::SimulatedOutput output;
    firmware::ActuatorBank actuators{output, config.stepper};
    firmware::MotionPlanner planner{config};
    firmware::PlaybackController player{planner, actuators};
    Receiver score;
    firmware::CommandReceiver commands{score, player, *this};
    DeviceEvents events;
    std::string responses;
    uint32_t now = 0;
    bool opened = false;

    VirtualDevice() { actuators.begin(); }
    void setEvents(DeviceEvents value) override { events = std::move(value); }
    std::vector<std::string> ports() const override { return {"VIRTUAL"}; }
    void open(const std::string&) override { opened = true; }
    void close() override { opened = false; }
    void startTimeout(int) override {}
    void stopTimeout() override {}
    void line(const char* text) override { responses += std::string(text) + '\n'; }
    bool write(std::string_view bytes) override {
        if (!opened) {
            return false;
        }
        for (const auto byte : bytes) {
            commands.accept(byte, now);
        }
        return true;
    }
    void pump() {
        while (!responses.empty()) {
            const auto end = responses.find('\n');
            const auto response = responses.substr(0, end + 1);
            responses.erase(0, end + 1);
            if (opened && events.received) {
                events.received(response);
            }
        }
    }
    void advance(uint32_t duration) {
        for (uint32_t elapsed = 0; elapsed < duration; elapsed += 10) {
            now += 10;
            commands.tick(now);
            pump();
        }
    }
};

int main() {
    VirtualDevice device;
    TransferSession session(device);
    session.connectPort("VIRTUAL");
    device.events.ready();
    device.pump();
    assert(session.state().phase == TransferPhase::Ready);
    assert(session.state().supportsPlayback && session.state().simulated);
    session.upload({"Test song", {{1, 3, 100}, {1, 0, 120}, {2, 1, 80}}});
    device.pump();
    assert(session.state().result == TransferResult::Loaded);
    assert(device.score.loaded && device.score.count == 3);
    session.play();
    device.pump();
    assert(session.state().phase == TransferPhase::Playing);
    device.advance(310000);
    assert(session.state().result == TransferResult::Played);
    assert(session.state().phase == TransferPhase::Ready);

    session.play();
    session.stop(); // STOP can be requested before OK PLAY arrives.
    device.pump();
    assert(session.state().result == TransferResult::Stopped);
    assert(!device.player.busy() && device.score.loaded);
    session.play();
    device.pump();
    session.disconnect();
    assert(device.opened); // Await the board's STOP acknowledgement before closing.
    device.pump();
    assert(!device.opened && !device.player.busy());
    assert(session.state().phase == TransferPhase::Disconnected);

    session.connectPort("VIRTUAL");
    device.events.ready();
    device.pump();
    session.upload({"Too fast", {{1, 1, 50}, {1, 24, 50}}});
    device.pump();
    session.play();
    device.pump();
    assert(session.state().error == TransferError::Playback);
    assert(session.state().phase == TransferPhase::Ready && device.opened);
    assert(device.score.loaded);
    session.play();
    session.stop();
    device.pump();
    assert(session.state().result == TransferResult::Stopped);
    assert(device.opened);

    session.upload({"Late loop", {{1, 0, 100}}});
    device.pump();
    session.play();
    device.pump();
    session.connectPort("ANOTHER");
    assert(session.state().phase == TransferPhase::Playing);
    device.now += 100000;
    device.commands.tick(device.now);
    device.pump();
    assert(session.state().error == TransferError::Playback);
    assert(session.state().details == "TIMING");
    std::cout << "PASS: app to firmware upload, simulation, completion, stop race, "
                 "disconnect, infeasible plan, asynchronous fault\n";
}
