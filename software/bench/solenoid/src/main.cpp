#include <Arduino.h>
#include <avr/interrupt.h>
#include <string.h>
#include <util/atomic.h>

#ifndef ARDUINO_AVR_UNO
#error "This test uses the Uno ATmega328P Timer1 and PD6 registers."
#endif

namespace {

constexpr uint8_t SOLENOID_PIN = 6;
constexpr uint16_t DEFAULT_PULSE_MS = 50;
constexpr uint16_t MIN_PULSE_MS = 10;
constexpr uint16_t MAX_PULSE_MS = 100;
constexpr uint32_t COOLDOWN_MS = 5000;
constexpr uint32_t INPUT_TIMEOUT_MS = 2000;
constexpr uint32_t TIMER_TICKS_PER_MS = F_CPU / 64 / 1000;
static_assert(TIMER_TICKS_PER_MS * MAX_PULSE_MS <= 65536UL,
              "Pulse must fit the 16-bit timer");

enum class Phase : uint8_t { Ready, Pulsing, Cooldown };
Phase phase = Phase::Ready;
volatile bool timerExpired = false;
uint32_t pulseStarted = 0;
uint32_t cooldownStarted = 0;
uint32_t lastByte = 0;
uint16_t pulseDuration = DEFAULT_PULSE_MS;
char input[32];
uint8_t inputLength = 0;
bool inputInvalid = false;

void outputOff() {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        TCCR1B = 0;
        TIMSK1 = 0;
        PORTD &= ~_BV(PD6);
        timerExpired = false;
    }
}

void stopPulse(uint32_t now) {
    outputOff();
    if (phase == Phase::Pulsing) {
        phase = Phase::Cooldown;
        cooldownStarted = now;
    }
}

void service(uint32_t now) {
    if (phase == Phase::Pulsing &&
        (timerExpired || uint32_t(now - pulseStarted) >= pulseDuration)) {
        stopPulse(now);
        Serial.println(F("DONE; output=OFF; cooldown_ms=5000"));
    }
    if (phase == Phase::Cooldown && uint32_t(now - cooldownStarted) >= COOLDOWN_MS) {
        phase = Phase::Ready;
        Serial.println(F("STATE READY"));
    }
}

void status(uint32_t now) {
    if (phase == Phase::Pulsing) {
        Serial.print(F("STATE PULSING "));
        Serial.println(pulseDuration);
    } else if (phase == Phase::Cooldown) {
        Serial.print(F("STATE COOLDOWN "));
        Serial.println(COOLDOWN_MS - uint32_t(now - cooldownStarted));
    } else {
        Serial.println(F("STATE READY"));
    }
}

void reject(const __FlashStringHelper* reason, uint32_t now) {
    stopPulse(now);
    Serial.print(F("ERR "));
    Serial.println(reason);
}

bool duration(const char* text, uint16_t& result) {
    if (!*text) {
        return false;
    }
    uint16_t value = 0;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9') {
            return false;
        }
        value = value * 10 + (*text - '0');
        if (value > MAX_PULSE_MS) {
            return false;
        }
    }
    result = value;
    return value >= MIN_PULSE_MS;
}

void startPulse(uint16_t milliseconds, uint32_t now) {
    if (phase != Phase::Ready) {
        Serial.println(F("ERR BUSY"));
        status(now);
        return;
    }
    pulseStarted = now;
    pulseDuration = milliseconds;
    phase = Phase::Pulsing;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        TCCR1A = 0;
        TCCR1B = 0;
        TCNT1 = 0;
        OCR1A = uint16_t(TIMER_TICKS_PER_MS * milliseconds - 1);
        TIFR1 = _BV(OCF1A);
        timerExpired = false;
        PORTD |= _BV(PD6);
        TIMSK1 = _BV(OCIE1A);
        TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10); // CTC, clock /64.
    }
    Serial.print(F("OK PULSE "));
    Serial.println(milliseconds);
}

void command(uint32_t now) {
    if (!strcmp(input, "STOP")) {
        stopPulse(now);
        Serial.println(F("OK STOP; output=OFF"));
        status(now);
    } else if (!strcmp(input, "STATUS")) {
        status(now);
    } else if (!strcmp(input, "PULSE")) {
        startPulse(DEFAULT_PULSE_MS, now);
    } else if (!strncmp(input, "PULSE ", 6)) {
        uint16_t milliseconds;
        if (duration(input + 6, milliseconds)) {
            startPulse(milliseconds, now);
        } else {
            reject(F("RANGE; use PULSE 10..100"), now);
        }
    } else {
        reject(F("COMMAND; use PULSE, PULSE 10..100, STOP, STATUS"), now);
    }
}

void accept(char byte, uint32_t now) {
    lastByte = now;
    if (byte == '\r') {
        return;
    }
    if (byte == '\n') {
        if (inputInvalid) {
            reject(F("LINE"), now);
        } else if (inputLength) {
            input[inputLength] = '\0';
            command(now);
        }
        inputLength = 0;
        inputInvalid = false;
    } else if (!byte || inputLength >= sizeof(input) - 1) {
        inputInvalid = true;
    } else if (!inputInvalid) {
        input[inputLength++] = byte;
    }
}

} // namespace

ISR(TIMER1_COMPA_vect) {
    // Independent of Serial and loop(): a one-shot hardware cutoff on D6.
    PORTD &= ~_BV(PD6);
    TCCR1B = 0;
    TIMSK1 = 0;
    timerExpired = true;
}

void setup() {
    digitalWrite(SOLENOID_PIN, LOW);
    pinMode(SOLENOID_PIN, OUTPUT);
    outputOff();
    // Leave the existing TMC2209 bench disabled if it is still connected.
    digitalWrite(4, HIGH);
    pinMode(4, OUTPUT);
    digitalWrite(2, LOW);
    pinMode(2, OUTPUT);
    phase = Phase::Ready;
    inputLength = 0;
    inputInvalid = false;
    Serial.begin(115200);
    Serial.println(F(
        "SOLENOID READY V1; D6; pulse=50ms; range=10..100ms; pause=5000ms; TIMER1"));
}

void loop() {
    service(millis());
    if ((inputLength || inputInvalid) &&
        uint32_t(uint32_t(millis()) - lastByte) >= INPUT_TIMEOUT_MS) {
        inputLength = 0;
        inputInvalid = false;
        reject(F("INPUT TIMEOUT"), millis());
    }
    for (uint8_t count = 0; count < 32 && Serial.available(); ++count) {
        service(millis());
        accept(Serial.read(), millis());
    }
    service(millis());
}
