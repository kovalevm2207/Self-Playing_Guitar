#include "protocol/command_receiver.h"

#include <stdio.h>
#include <string.h>

namespace guitar {
namespace firmware {
namespace {

bool number(const char* text, uint16_t& result) {
    if (!text || !*text) {
        return false;
    }
    uint32_t value = 0;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9') {
            return false;
        }
        value = value * 10 + (*text - '0');
        if (value > 65535) {
            return false;
        }
    }
    result = uint16_t(value);
    return true;
}

} // namespace

void CommandReceiver::error(const char* reason, bool discard) {
    m_player.stop();
    if (discard) {
        m_score.abort();
    }
    char response[32];
    snprintf(response, sizeof(response), "ERR %s", reason);
    m_output.line(response);
}

void CommandReceiver::status() {
    const auto phase = m_player.phase();
    const char* state = m_player.busy()                     ? "PLAYING"
                        : phase == PlaybackPhase::Fault     ? "FAULT"
                        : phase == PlaybackPhase::Completed ? "DONE"
                        : m_score.loaded                    ? "LOADED"
                                                            : "EMPTY";
    char response[64];
    snprintf(response,
             sizeof(response),
             "STATUS %s %u %u SIMULATED",
             state,
             unsigned(m_player.noteIndex()),
             unsigned(m_score.count));
    m_output.line(response);
}

void CommandReceiver::command(uint32_t now) {
    char* tokens[7];
    uint8_t count = 0;
    char* token = strtok(m_buffer, " ");
    while (token && count < 7) {
        tokens[count++] = token;
        token = strtok(nullptr, " ");
    }
    if (!count) {
        return;
    }
    if (!strcmp(tokens[0], "HELLO") && count == 2 && !strcmp(tokens[1], "1")) {
        m_player.stop();
        m_score.abort();
        char response[40];
        snprintf(response,
                 sizeof(response),
                 "READY 1 %u SIMULATED %u",
                 unsigned(MaxNotes),
                 unsigned(m_player.supportedStringMask()));
        m_output.line(response);
        return;
    }
    if (!strcmp(tokens[0], "STOP") && count == 1) {
        m_player.stop();
        m_output.line("OK STOP");
        return;
    }
    if (!strcmp(tokens[0], "ABORT") && count == 1) {
        m_player.stop();
        m_score.abort();
        m_output.line("OK ABORT");
        return;
    }
    if (!strcmp(tokens[0], "STATUS") && count == 1) {
        status();
        return;
    }
    if (m_player.busy()) {
        // Rejected uploads must not overwrite the sequence being executed.
        m_output.line("ERR BUSY");
        return;
    }
    if (!strcmp(tokens[0], "PLAY") && count == 1) {
        if (!m_score.loaded) {
            error("NOT_LOADED", false);
        } else if (!m_player.play(m_score.notes, m_score.count, now)) {
            error("PLAN", false);
        } else {
            m_output.line("OK PLAY");
        }
        return;
    }
    if (!strcmp(tokens[0], "BEGIN") && count == 3) {
        uint16_t noteCount = 0;
        uint16_t checksum = 0;
        if (!number(tokens[1], noteCount) || !number(tokens[2], checksum)) {
            error("COMMAND");
        } else if (m_score.begin(noteCount, checksum)) {
            m_player.stop();
            m_output.line("OK BEGIN");
        } else {
            error("CAPACITY");
        }
        return;
    }
    if (!strcmp(tokens[0], "NOTE") && count == 5) {
        uint16_t index = 0, string = 0, fret = 0, duration = 0;
        if (!number(tokens[1], index) || !number(tokens[2], string) ||
            !number(tokens[3], fret) || !number(tokens[4], duration)) {
            error("COMMAND");
        } else if (!string || string > 6 || fret > 24) {
            error("NOTE");
        } else if (!(m_player.supportedStringMask() & (1 << (string - 1)))) {
            error("STRING");
        } else if (!m_score.append(index,
                                   {uint8_t(string), uint8_t(fret), duration})) {
            error("NOTE");
        } else {
            char response[24];
            snprintf(response, sizeof(response), "OK %u", unsigned(index));
            m_output.line(response);
        }
        return;
    }
    if (!strcmp(tokens[0], "END") && count == 1) {
        if (m_score.end()) {
            m_output.line("OK END");
        } else {
            error("CHECKSUM");
        }
        return;
    }
    error("COMMAND");
}

void CommandReceiver::accept(char byte, uint32_t now) {
    m_lastByte = now;
    if (byte == '\r') {
        return;
    }
    if (byte == '\n') {
        if (m_overflow) {
            error("LINE_TOO_LONG");
        } else {
            m_buffer[m_length] = '\0';
            command(now);
        }
        m_length = 0;
        m_overflow = false;
    } else if (m_length < sizeof(m_buffer) - 1 && !m_overflow) {
        m_buffer[m_length++] = byte;
    } else {
        m_overflow = true;
    }
}

void CommandReceiver::tick(uint32_t now) {
    if ((m_score.receiving || m_length || m_overflow) &&
        uint32_t(now - m_lastByte) > 5000000UL) {
        m_length = 0;
        m_overflow = false;
        error("TIMEOUT");
    }
    const bool wasPlaying = m_player.busy();
    m_player.tick(now);
    if (wasPlaying && m_player.phase() == PlaybackPhase::Completed) {
        m_output.line("EVENT DONE");
    } else if (wasPlaying && m_player.phase() == PlaybackPhase::Fault) {
        m_output.line("EVENT FAULT TIMING");
    }
}

} // namespace firmware
} // namespace guitar
