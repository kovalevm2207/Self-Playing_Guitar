#include <Arduino.h>
#ifdef UART_USE_ALT
#include <AltSoftSerial.h>
AltSoftSerial tmc;
#else
#include <SoftwareSerial.h>
SoftwareSerial tmc(8, 9);
#endif

// Pure read-only UART test. No writes to TMC registers, no STEP pulses,
// no DIAG interrupts, no AltSoftSerial and no motion timer.
// D8 RX -> selected PDN_UART; D9 TX -> 1k -> same PDN_UART.
// EN D4 is HIGH throughout. Supply both VIO and VM normally.
constexpr uint32_t BAUDS[] = {9600, 19200, 38400, 57600};

uint8_t packetCrc(const uint8_t *bytes, uint8_t size) {
  uint8_t result = 0;
  for (uint8_t i = 0; i < size; ++i) {
    uint8_t current = bytes[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      const bool feedback = (result >> 7) ^ (current & 1);
      result <<= 1;
      if (feedback) result ^= 7;
      current >>= 1;
    }
  }
  return result;
}

void printHex(const uint8_t *bytes, uint8_t count) {
  for (uint8_t i = 0; i < count; ++i) {
    if (bytes[i] < 16) Serial.print('0');
    Serial.print(bytes[i], HEX);
    Serial.print(' ');
  }
}

bool readIdentity(uint8_t address) {
  while (tmc.available()) tmc.read();
  uint8_t request[] = {0x05, address, 0x06, 0}; // IOIN, read-only.
  request[3] = packetCrc(request, 3);
  uint8_t received[32], count = 0;
  Serial.print(F("  address=")); Serial.print(address);
  Serial.print(F("; TX=")); printHex(request, 4);
  // Ensure USB logging is drained before receiving from the driver.
  Serial.flush();
  tmc.write(request, 4);
  const uint32_t began = millis();
  while (millis() - began < 60) {
    if (tmc.available()) {
      const uint8_t byte = tmc.read();
      if (count < sizeof(received)) received[count++] = byte;
    }
  }
  Serial.print(F("; RX=")); printHex(received, count);
  for (uint8_t offset = 0; offset + 8 <= count; ++offset) {
    const uint8_t *reply = received + offset;
    if (reply[0] == 0x05 && reply[1] == 0xFF && reply[2] == 0x06
        && packetCrc(reply, 7) == reply[7]) {
      const uint32_t ioin = static_cast<uint32_t>(reply[3]) << 24
          | static_cast<uint32_t>(reply[4]) << 16 | static_cast<uint32_t>(reply[5]) << 8 | reply[6];
      Serial.print(F("; VALID CRC; IOIN=0x")); Serial.print(ioin, HEX);
      Serial.print(F("; VERSION=0x")); Serial.print(reply[3], HEX);
      Serial.println(reply[3] == 0x21 ? F("; TMC2209 FOUND") : F("; OTHER CHIP VERSION"));
      return reply[3] == 0x21;
    }
  }
  Serial.println(count ? F("; NO VALID REPLY") : F("(none); NO REPLY"));
  return false;
}

void scan() {
  bool found = false;
#ifdef UART_USE_ALT
  Serial.println(F("UART_MIN START; AltSoftSerial RX=D8 TX=D9; EN=D4 HIGH; NO MOTION"));
#else
  Serial.println(F("UART_MIN START; SoftwareSerial RX=D8 TX=D9; EN=D4 HIGH; NO MOTION"));
#endif
  for (uint8_t speed = 0; speed < sizeof(BAUDS) / sizeof(BAUDS[0]); ++speed) {
    tmc.end();
    tmc.begin(BAUDS[speed]);
#ifndef UART_USE_ALT
    tmc.listen();
#endif
    delay(20); // Idle time allows TMC UART parser to recover/resynchronize.
    Serial.print(F("BAUD=")); Serial.println(BAUDS[speed]);
    for (uint8_t address = 0; address < 4; ++address) {
      // Repeat each address to distinguish a transient from persistent silence.
      for (uint8_t attempt = 0; attempt < 2; ++attempt) {
        if (readIdentity(address)) found = true;
        delay(10);
      }
    }
  }
  Serial.println(found ? F("UART_MIN RESULT: TMC2209 FOUND; save working baud/address")
                       : F("UART_MIN RESULT: NO TMC2209 REPLY; inspect module UART path/jumper"));
  Serial.println(F("Send s + Enter to scan again. SoftwareSerial may NOT show TX echo; this is normal."));
}

void setup() {
  digitalWrite(4, HIGH);
  pinMode(4, OUTPUT);
  digitalWrite(2, LOW);
  pinMode(2, OUTPUT);
  digitalWrite(3, LOW);
  pinMode(3, OUTPUT);
  Serial.begin(115200);
  delay(1500);
  scan();
}

void loop() {
  if (Serial.available() && Serial.read() == 's') scan();
}
