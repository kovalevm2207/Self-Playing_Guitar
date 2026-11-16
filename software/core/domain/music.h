#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "score.h"

namespace guitar {

struct Song {
    std::string id;
    std::string title;
    std::string author;
};

struct Arrangement {
    std::string id;
    std::string songId;
    std::string label;
    std::string description;
    int tempo = 100;
    std::vector<Note> notes;
};

struct PreparedScore {
    std::string title;
    std::vector<Note> notes;
};

bool valid(const Arrangement& arrangement);
std::vector<int> usedStrings(const Arrangement& arrangement);
uint64_t durationMs(const Arrangement& arrangement);
PreparedScore prepareScore(const Arrangement& arrangement, const std::string& title,
                           int tempo);

} // namespace guitar
