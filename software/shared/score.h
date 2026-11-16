#pragma once

#include <stddef.h>
#include <stdint.h>

namespace guitar {

constexpr uint16_t MaxNotes = 96;

struct Note {
    uint8_t string;
    uint8_t fret;
    uint16_t duration;
};

inline bool valid(const Note& note) {
    return note.string >= 1 && note.string <= 6 && note.fret <= 24 &&
           note.duration >= 50 && note.duration <= 10000;
}

inline uint16_t crcByte(uint16_t crc, uint8_t value) {
    crc ^= uint16_t(value) << 8;
    for (uint8_t i = 0; i < 8; ++i) {
        crc = (crc & 0x8000) ? uint16_t((crc << 1) ^ 0x1021) : uint16_t(crc << 1);
    }
    return crc;
}

inline uint16_t addNote(uint16_t crc, const Note& note) {
    crc = crcByte(crc, note.string);
    crc = crcByte(crc, note.fret);
    crc = crcByte(crc, uint8_t(note.duration));
    crc = crcByte(crc, uint8_t(note.duration >> 8));
    return crc;
}

inline uint8_t stringMask(const Note* notes, size_t count) {
    uint8_t mask = 0;
    for (size_t i = 0; i < count; ++i) {
        if (valid(notes[i])) {
            mask |= 1 << (notes[i].string - 1);
        }
    }
    return mask;
}

// No heap allocation; the same receiver is tested on desktop and used on Uno.
class Receiver {
public:
    Note notes[MaxNotes]{};
    uint16_t count = 0;
    uint16_t received = 0;
    uint16_t expectedCrc = 0;
    uint16_t crc = 0xffff;
    bool receiving = false;
    bool loaded = false;

    void abort() {
        count = received = 0;
        receiving = loaded = false;
    }

    bool begin(uint16_t size, uint16_t checksum) {
        abort();
        if (!size || size > MaxNotes) {
            return false;
        }
        count = size;
        expectedCrc = checksum;
        crc = 0xffff;
        receiving = true;
        return true;
    }

    bool append(uint16_t index, Note note) {
        if (!receiving || index != received || received >= count || !valid(note)) {
            return false;
        }
        notes[received++] = note;
        crc = addNote(crc, note);
        return true;
    }

    bool end() {
        if (!receiving || received != count || crc != expectedCrc) {
            abort();
            return false;
        }
        receiving = false;
        loaded = true;
        return true;
    }
};

} // namespace guitar
