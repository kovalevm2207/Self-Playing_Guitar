#include "application/transfer_session.h"

#include <algorithm>
#include <sstream>
#include <utility>

namespace guitar {
namespace {

constexpr int RESPONSE_TIMEOUT_MS = 3000;
constexpr size_t MAX_RESPONSE_BYTES = 4096;

std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos) {
        return {};
    }
    return value.substr(first, value.find_last_not_of(" \t\r") - first + 1);
}

} // namespace

TransferSession::TransferSession(DeviceTransport& transport)
    : m_transport(transport) {
    DeviceEvents events;
    events.ready = [this] {
        if (m_state.phase == TransferPhase::Connecting) {
            send("HELLO 1", "READY");
        }
    };
    events.received = [this](std::string bytes) { receive(std::move(bytes)); };
    events.failed = [this](std::string error) {
        fail(TransferError::Connection, std::move(error));
    };
    events.timedOut = [this] {
        if (!m_expected.empty()) {
            fail(TransferError::Timeout);
        }
    };
    m_transport.setEvents(std::move(events));
}

TransferSession::~TransferSession() {
    m_transport.setEvents({});
    m_transport.close();
}

void TransferSession::setOnChanged(std::function<void()> callback) {
    m_onChanged = std::move(callback);
}

void TransferSession::changed() {
    if (m_onChanged) {
        m_onChanged();
    }
}

void TransferSession::reset() {
    ++m_generation;
    m_transport.stopTimeout();
    m_transport.close();
    m_state = {};
    m_expected.clear();
    m_input.clear();
    m_score = {};
    m_nextNote = 0;
    m_capacity = 0;
}

void TransferSession::connectPort(const std::string& port) {
    reset();
    if (port.empty()) {
        fail(TransferError::PortRequired);
        return;
    }
    m_state.phase = TransferPhase::Connecting;
    const auto generation = m_generation;
    changed();
    // A synchronous observer can cancel or replace this connection attempt.
    if (generation == m_generation && m_state.phase == TransferPhase::Connecting) {
        m_transport.open(port);
    }
}

void TransferSession::disconnect() {
    reset();
    changed();
}

void TransferSession::fail(TransferError error, std::string details) {
    reset();
    m_state.error = error;
    m_state.details = std::move(details);
    changed();
}

void TransferSession::send(const std::string& line, std::string expected) {
    m_expected = std::move(expected);
    if (!m_transport.write(line + '\n')) {
        if (m_state.error == TransferError::None) {
            fail(TransferError::Connection);
        }
        return;
    }
    // A synchronous transport failure may already have reset the session.
    if (!m_expected.empty()) {
        m_transport.startTimeout(RESPONSE_TIMEOUT_MS);
    }
}

void TransferSession::receive(std::string bytes) {
    m_input += bytes;
    if (m_input.size() > MAX_RESPONSE_BYTES) {
        fail(TransferError::Protocol);
        return;
    }
    size_t end;
    while ((end = m_input.find('\n')) != std::string::npos) {
        auto line = trim(m_input.substr(0, end));
        m_input.erase(0, end + 1);
        handle(line);
    }
}

void TransferSession::handle(const std::string& line) {
    if (line == "BOOT 1") {
        if (m_state.phase == TransferPhase::Ready ||
            m_state.phase == TransferPhase::Uploading) {
            fail(TransferError::Protocol, "Board restarted");
        }
        return;
    }
    if (m_expected.empty()) {
        return;
    }
    if (line.rfind("ERR ", 0) == 0) {
        fail(TransferError::DeviceRejected, line.substr(4));
        return;
    }
    if (m_expected == "READY") {
        std::istringstream response(line);
        std::string marker;
        std::string version;
        std::string mode;
        std::string extra;
        int capacity = 0;
        if (!(response >> marker >> version >> capacity >> mode) ||
            response >> extra || marker != "READY" || version != "1" ||
            mode != "DRYRUN" || capacity < 1) {
            fail(TransferError::Protocol);
            return;
        }
        m_transport.stopTimeout();
        m_expected.clear();
        m_capacity = std::min<size_t>(capacity, MaxNotes);
        m_state.phase = TransferPhase::Ready;
        changed();
        return;
    }

    if (line != m_expected) {
        fail(TransferError::Protocol);
        return;
    }
    m_transport.stopTimeout();
    const auto acknowledged = std::exchange(m_expected, {});
    if (acknowledged == "OK END") {
        m_state.phase = TransferPhase::Ready;
        m_state.result = TransferResult::Loaded;
        m_state.progress = 1;
        m_state.loadedTitle = m_score.title;
        changed();
        return;
    }
    if (acknowledged == "OK BEGIN") {
        m_nextNote = 0;
    } else {
        ++m_nextNote;
    }
    m_state.progress = double(m_nextNote) / m_score.notes.size();
    next();
    changed();
}

void TransferSession::reportInvalidScore(const std::string& details) {
    if (m_state.phase != TransferPhase::Ready) {
        return;
    }
    m_state.error = TransferError::InvalidScore;
    m_state.details = details;
    changed();
}

void TransferSession::upload(PreparedScore score) {
    if (m_state.phase != TransferPhase::Ready) {
        return;
    }
    if (score.notes.empty() ||
        !std::all_of(score.notes.begin(), score.notes.end(), [](const Note& note) {
            return valid(note);
        })) {
        reportInvalidScore("Invalid notes");
        return;
    }
    if (score.notes.size() > m_capacity) {
        m_state.error = TransferError::Capacity;
        changed();
        return;
    }

    m_score = std::move(score);
    uint16_t checksum = 0xffff;
    for (const auto& note : m_score.notes) {
        checksum = addNote(checksum, note);
    }

    m_state = {};
    m_state.phase = TransferPhase::Uploading;
    send("BEGIN " + std::to_string(m_score.notes.size()) + ' ' +
             std::to_string(checksum),
         "OK BEGIN");
    changed();
}

void TransferSession::next() {
    if (m_nextNote == m_score.notes.size()) {
        send("END", "OK END");
        return;
    }
    const auto& note = m_score.notes[m_nextNote];
    send("NOTE " + std::to_string(m_nextNote) + ' ' + std::to_string(note.string) +
             ' ' + std::to_string(note.fret) + ' ' + std::to_string(note.duration),
         "OK " + std::to_string(m_nextNote));
}

void TransferSession::cancel() {
    if (m_state.phase != TransferPhase::Uploading &&
        m_state.phase != TransferPhase::Connecting) {
        return;
    }
    reset();
    m_state.result = TransferResult::Cancelled;
    changed();
}

} // namespace guitar
