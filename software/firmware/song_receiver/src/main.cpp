#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#include "../../../shared/score.h"

guitar::Receiver receiver;
char buffer[80];
uint8_t length = 0;
bool overflow = false;
unsigned long lastByte = 0;

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

void error(const __FlashStringHelper* message) {
    receiver.abort();
    Serial.print(F("ERR "));
    Serial.println(message);
}

void command() {
    char* tokens[7];
    uint8_t count = 0;
    char* token = strtok(buffer, " ");
    while (token && count < 7) {
        tokens[count++] = token;
        token = strtok(nullptr, " ");
    }
    if (!count) {
        return;
    }
    if (!strcmp(tokens[0], "HELLO") && count == 2 && !strcmp(tokens[1], "1")) {
        receiver.abort();
        Serial.println(F("READY 1 96 DRYRUN"));
        return;
    }
    if (!strcmp(tokens[0], "ABORT") && count == 1) {
        receiver.abort();
        Serial.println(F("OK ABORT"));
        return;
    }

    if (!strcmp(tokens[0], "BEGIN") && count == 3) {
        uint16_t noteCount = 0;
        uint16_t checksum = 0;
        if (!number(tokens[1], noteCount) || !number(tokens[2], checksum)) {
            error(F("COMMAND"));
            return;
        }

        if (receiver.begin(noteCount, checksum)) {
            Serial.println(F("OK BEGIN"));
        } else {
            error(F("CAPACITY"));
        }
        return;
    }

    if (!strcmp(tokens[0], "NOTE") && count == 5) {
        uint16_t noteIndex = 0;
        uint16_t stringNumber = 0;
        uint16_t fret = 0;
        uint16_t durationMs = 0;
        if (!number(tokens[1], noteIndex) || !number(tokens[2], stringNumber) ||
            !number(tokens[3], fret) || !number(tokens[4], durationMs)) {
            error(F("COMMAND"));
            return;
        }

        if (stringNumber > 6 || fret > 24 ||
            !receiver.append(noteIndex,
                             {uint8_t(stringNumber), uint8_t(fret), durationMs})) {
            error(F("NOTE"));
            return;
        }
        Serial.print(F("OK "));
        Serial.println(noteIndex);
        return;
    }
    if (!strcmp(tokens[0], "END") && count == 1) {
        if (receiver.end()) {
            Serial.println(F("OK END"));
        } else {
            error(F("CHECKSUM"));
        }
        return;
    }
    error(F("COMMAND"));
}

void setup() {
    Serial.begin(115200);
    Serial.println(F("BOOT 1"));
}

void loop() {
    if ((receiver.receiving || length || overflow) && millis() - lastByte > 5000) {
        length = 0;
        overflow = false;
        error(F("TIMEOUT"));
    }
    while (Serial.available()) {
        char ch = Serial.read();
        lastByte = millis();
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            if (overflow) {
                error(F("LINE_TOO_LONG"));
            } else {
                buffer[length] = '\0';
                command();
            }
            length = 0;
            overflow = false;
        } else if (length < sizeof(buffer) - 1 && !overflow) {
            buffer[length++] = ch;
        } else {
            overflow = true;
        }
    }
}
