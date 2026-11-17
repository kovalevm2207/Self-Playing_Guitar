#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "application/ports.h"

namespace guitar {

enum class TransferPhase {
    Disconnected,
    Connecting,
    Ready,
    Uploading,
    Starting,
    Playing,
    Stopping
};
enum class TransferResult { None, Loaded, Cancelled, Played, Stopped };
enum class TransferError {
    None,
    PortRequired,
    Connection,
    Timeout,
    Protocol,
    DeviceRejected,
    Capacity,
    InvalidScore,
    Playback
};

struct TransferState {
    TransferPhase phase = TransferPhase::Disconnected;
    TransferResult result = TransferResult::None;
    TransferError error = TransferError::None;
    std::string details;
    std::string loadedTitle;
    double progress = 0;
    bool supportsPlayback = false;
    bool simulated = false;
};

class TransferSession {
public:
    explicit TransferSession(DeviceTransport& transport);
    ~TransferSession();
    TransferSession(const TransferSession&) = delete;
    TransferSession& operator=(const TransferSession&) = delete;

    const TransferState& state() const { return m_state; }
    void setOnChanged(std::function<void()> callback);
    void connectPort(const std::string& port);
    void disconnect();
    void upload(PreparedScore score);
    void cancel();
    void play();
    void stop();
    void reportInvalidScore(const std::string& details);

private:
    void changed();
    void reset();
    void fail(TransferError error, std::string details = {});
    void send(const std::string& line, std::string expected);
    void receive(std::string bytes);
    void handle(const std::string& line);
    void next();

    DeviceTransport& m_transport;
    TransferState m_state;
    std::function<void()> m_onChanged;
    PreparedScore m_score;
    std::string m_input;
    std::string m_expected;
    size_t m_nextNote = 0;
    size_t m_capacity = 0;
    uint64_t m_generation = 0;
    bool m_disconnectAfterStop = false;
};

} // namespace guitar
