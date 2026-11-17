#undef NDEBUG
#include <cassert>
#include <iostream>

#include "../firmware/song_receiver/src/main.cpp"
std::string transmit(const std::string& text) {
    Serial.input = text;
    Serial.output.clear();
    do {
        loop();
    } while (Serial.available());
    return Serial.output;
}
int main() {
    setup();
    assert(Serial.output == "BOOT 1\n");
    assert(transmit("HELLO 1\r\n") == "READY 1 96 SIMULATED\n");
    const auto crc = guitar::addNote(0xffff, {1, 3, 500});
    const std::string begin = "BEGIN 1 " + std::to_string(crc) + "\n";
    assert(transmit(begin) == "OK BEGIN\n");
    assert(transmit("NOTE 0 1 3 500\nEND\n") == "OK 0\nOK END\n");
    assert(receiver.loaded);
    assert(transmit("ABORT\n") == "OK ABORT\n");
    assert(!receiver.loaded);
    assert(transmit("BEGIN 97 0\n") == "ERR CAPACITY\n");
    assert(transmit(begin + "NOTE 1 1 3 500\n") == "OK BEGIN\nERR NOTE\n");
    assert(transmit(begin + "NOTE 0 257 3 500\n") == "OK BEGIN\nERR NOTE\n");
    assert(transmit(begin + "NOTE 0 1 3 -1\n") == "OK BEGIN\nERR COMMAND\n");
    assert(transmit(begin + "END\n") == "OK BEGIN\nERR CHECKSUM\n");
    assert(transmit("BEGIN 1 0\nNOTE 0 1 3 500\nEND\n") ==
           "OK BEGIN\nOK 0\nERR CHECKSUM\n");
    assert(transmit(std::string(100, 'x') + "\nHELLO 1\n") ==
           "ERR LINE_TOO_LONG\nREADY 1 96 SIMULATED\n");
    assert(transmit("BEGIN 655360 0\n") == "ERR COMMAND\n");
    assert(transmit(begin + "NOTE 0 1 ") == "OK BEGIN\n");
    testTime += 5001;
    Serial.output.clear();
    loop();
    assert(Serial.output == "ERR TIMEOUT\n");
    assert(!receiver.loaded && !receiver.receiving);
    assert(transmit("HELLO 1\n") == "READY 1 96 SIMULATED\n");
    assert(transmit("PLAY\n") == "ERR NOT_LOADED\n");
    assert(transmit(begin + "NOTE 0 1 3 500\nEND\nPLAY\n") ==
           "OK BEGIN\nOK 0\nOK END\nOK PLAY\n");
    assert(player.busy());
    assert(transmit("STATUS\n") == "STATUS PLAYING 0 1 SIMULATED\n");
    assert(transmit("BEGIN 1 0\n") == "ERR BUSY\n");
    assert(receiver.loaded && receiver.notes[0].fret == 3);
    assert(transmit("STOP\n") == "OK STOP\n");
    assert(!player.busy() && receiver.loaded);
    assert(transmit("STATUS\n") == "STATUS LOADED 0 1 SIMULATED\n");
    assert(transmit("PLAY\n") == "OK PLAY\n");
    testTime += 100;
    Serial.output.clear();
    loop();
    assert(Serial.output == "EVENT FAULT TIMING\n");
    assert(transmit("HELLO 1\n") == "READY 1 96 SIMULATED\n");
    assert(gpioWrites == 0); // The default profile must never touch board outputs.
    std::cout << "PASS: actual Uno parser, handshake, transfer, CRC, overflow, "
                 "numeric bounds, timeout recovery\n";
}
