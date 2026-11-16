#include "domain/music.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace guitar {

bool valid(const Arrangement& arrangement) {
    return !arrangement.id.empty() && !arrangement.songId.empty() &&
           arrangement.tempo >= 40 && arrangement.tempo <= 220 &&
           !arrangement.notes.empty() &&
           std::all_of(arrangement.notes.begin(),
                       arrangement.notes.end(),
                       [](const Note& note) { return valid(note); });
}

std::vector<int> usedStrings(const Arrangement& arrangement) {
    std::vector<int> result;
    const auto mask = stringMask(arrangement.notes.data(), arrangement.notes.size());
    for (int stringNumber = 1; stringNumber <= 6; ++stringNumber) {
        if (mask & (1 << (stringNumber - 1))) {
            result.push_back(stringNumber);
        }
    }
    return result;
}

uint64_t durationMs(const Arrangement& arrangement) {
    uint64_t duration = 0;
    for (const auto& note : arrangement.notes) {
        duration += note.duration;
    }
    return duration;
}

PreparedScore prepareScore(const Arrangement& arrangement, const std::string& title,
                           int tempo) {
    if (!valid(arrangement) || tempo < 40 || tempo > 220) {
        throw std::invalid_argument("Invalid arrangement or tempo");
    }

    PreparedScore score;
    score.title = title;
    score.notes.reserve(arrangement.notes.size());
    for (auto note : arrangement.notes) {
        const auto duration =
            std::lround(note.duration * double(arrangement.tempo) / tempo);
        if (duration < 50 || duration > 10000) {
            throw std::invalid_argument("Note duration is outside protocol limits");
        }
        note.duration = static_cast<uint16_t>(duration);
        score.notes.push_back(note);
    }
    return score;
}

} // namespace guitar
