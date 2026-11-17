#pragma once

#include "playback/playback_controller.h"
#include "score.h"

namespace guitar {
namespace firmware {

class ResponseOutput {
public:
    virtual ~ResponseOutput() = default;
    virtual void line(const char* text) = 0;
};

class CommandReceiver {
public:
    CommandReceiver(Receiver& score, PlaybackController& player,
                    ResponseOutput& output)
        : m_score(score), m_player(player), m_output(output) {}
    void accept(char byte, uint32_t now);
    void tick(uint32_t now);

private:
    void command(uint32_t now);
    void error(const char* reason, bool discard = true);
    void status();

    Receiver& m_score;
    PlaybackController& m_player;
    ResponseOutput& m_output;
    char m_buffer[80]{};
    uint32_t m_lastByte = 0;
    uint8_t m_length = 0;
    bool m_overflow = false;
};

} // namespace firmware
} // namespace guitar
