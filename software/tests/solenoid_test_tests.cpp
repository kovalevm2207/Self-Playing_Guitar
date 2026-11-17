#undef NDEBUG
#include <cassert>
#include <iostream>

#include "../bench/solenoid/src/main.cpp"

bool outputHigh() {
    return PORTD & _BV(PD6);
}

std::string transmit(const std::string& bytes) {
    Serial.input = bytes;
    Serial.output.clear();
    do {
        loop();
    } while (Serial.available());
    return Serial.output;
}

void reset(uint32_t now = 0) {
    testTime = now;
    Serial.input.clear();
    Serial.output.clear();
    setup();
    assert(!outputHigh() && !TCCR1B && !TIMSK1);
    assert(Serial.output.find("SOLENOID READY V1") == 0);
    assert(gpioLevels[4] == HIGH && gpioLevels[2] == LOW);
}

void checkSinglePulse() {
    reset();
    assert(transmit("PULSE\r\n") == "OK PULSE 50\n");
    assert(outputHigh() && phase == Phase::Pulsing);
    assert(OCR1A == 12499 && TCNT1 == 0);
    assert(TCCR1B == (_BV(WGM12) | _BV(CS11) | _BV(CS10)));
    testTime = 49;
    assert(transmit("PULSE 100\n").find("ERR BUSY") == 0);
    assert(pulseDuration == 50 && OCR1A == 12499);
    // Run the actual cutoff ISR, without giving the main loop any CPU time.
    testTime = 50;
    TIMER1_COMPA_vect();
    assert(!outputHigh() && !TCCR1B && !TIMSK1 && timerExpired);
    Serial.output.clear();
    loop();
    assert(phase == Phase::Cooldown);
    assert(Serial.output == "DONE; output=OFF; cooldown_ms=5000\n");
    const auto started = cooldownStarted;
    testTime = 100;
    assert(transmit("STOP\n").find("OK STOP") == 0);
    assert(cooldownStarted == started); // STOP cannot bypass or extend cooldown.
    testTime = 5049;
    assert(transmit("PULSE\n").find("ERR BUSY") == 0);
    testTime = 5050;
    assert(transmit("PULSE 100\n").find("OK PULSE 100") != std::string::npos);
    assert(outputHigh() && OCR1A == 24999);
    assert(transmit("STOP\n").find("OK STOP") == 0);
    assert(!outputHigh() && phase == Phase::Cooldown && !TIMSK1);
}

void checkInput() {
    for (const std::string command : {"PULSE 0",
                                      "PULSE 9",
                                      "PULSE 101",
                                      "PULSE -20",
                                      "PULSE 20x",
                                      "PULSE 20 50",
                                      "PULSE 999999999999",
                                      "PULSE ",
                                      "ON"}) {
        reset();
        assert(transmit(command + '\n').find("ERR ") == 0);
        assert(!outputHigh() && phase == Phase::Ready);
    }
    reset();
    assert(transmit("PULSE 10\n") == "OK PULSE 10\n");
    testTime = 10;
    assert(transmit("STATUS\n").find("STATE COOLDOWN") != std::string::npos);
    assert(!outputHigh()); // Software deadline backs up the timer.

    reset();
    assert(transmit("PULSE 50").empty() && !outputHigh());
    testTime = 2000;
    assert(transmit("").find("ERR INPUT TIMEOUT") == 0);
    assert(!outputHigh());
    assert(transmit(std::string(60, 'x') + "\n").find("ERR LINE") == 0);
    assert(transmit(std::string("PULSE\0junk\n", 11)).find("ERR LINE") == 0);
    assert(transmit("PULSE\nPULSE\nPULSE\n").find("ERR BUSY") != std::string::npos);
    assert(outputHigh());
    assert(transmit("BAD\n").find("ERR COMMAND") == 0);
    assert(!outputHigh() && phase == Phase::Cooldown);
}

void checkClockWrapAndBoot() {
    reset(0xfffffff0U);
    transmit("PULSE 20\n");
    testTime = uint32_t(0xfffffff0U + 20U);
    loop();
    assert(!outputHigh() && phase == Phase::Cooldown);
    testTime = uint32_t(0xfffffff0U + 5019U);
    loop();
    assert(phase == Phase::Cooldown);
    testTime = uint32_t(0xfffffff0U + 5020U);
    loop();
    assert(phase == Phase::Ready);
    transmit("PULSE\n");
    assert(outputHigh());
    reset();
    assert(!outputHigh() && phase == Phase::Ready);
    assert(transmit("STATUS\n") == "STATE READY\n");
}

int main() {
    checkSinglePulse();
    checkInput();
    checkClockWrapAndBoot();
    std::cout
        << "PASS: solenoid startup OFF, Timer1 cutoff, pulse limits, cooldown, "
           "STOP, malformed input, partial lines, clock wrap, reset\n";
}
