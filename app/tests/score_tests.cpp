#include "../core/score.h"
#undef NDEBUG
#include <cassert>
#include <iostream>
int main() {
    using namespace guitar;
    uint16_t crc = 0xffff;
    for (char c : "123456789") if (c) crc = crcByte(crc, c);
    assert(crc == 0x29b1); // CRC-16/CCITT-FALSE known vector.
    Note notes[] = {{1,0,500},{3,12,250},{1,24,10000}};
    crc = 0xffff; for (auto n : notes) crc = addNote(crc, n);
    assert(stringMask(notes, 3) == 5);
    Receiver r;
    assert(!r.begin(0, crc)); assert(!r.begin(97, crc));
    assert(r.begin(3, crc)); assert(!r.append(1, notes[0]));
    for (int i = 0; i < 3; ++i) assert(r.append(i, notes[i]));
    assert(!r.append(3, notes[0])); assert(r.end()); assert(r.loaded);
    assert(r.begin(3, crc)); assert(!r.loaded); assert(!r.end()); assert(!r.loaded);
    assert(r.begin(1, 0)); assert(r.append(0, notes[0])); assert(!r.end());
    assert(r.begin(1, crc)); assert(!r.append(0, {0,0,500})); assert(!r.append(0, {7,0,500}));
    assert(!r.append(0, {1,25,500})); assert(!r.append(0, {1,0,49})); assert(!r.append(0, {1,0,10001}));
    r.abort(); assert(!r.receiving && !r.loaded && r.count == 0);
    Note boundary{6,24,50}; crc = 0xffff;
    for (int i = 0; i < MaxNotes; ++i) crc = addNote(crc, boundary);
    assert(r.begin(MaxNotes, crc));
    for (int i = 0; i < MaxNotes; ++i) assert(r.append(i, boundary));
    assert(r.end());
    std::cout << "PASS: CRC, limits, string mask, ordering, truncation, corruption, abort, full capacity\n";
}
