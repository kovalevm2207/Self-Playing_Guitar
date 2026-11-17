"""Run the actual motion loop with mocked AVR hardware, including an ISR race.

Only the dispatch boundary receives an injected interrupt hook. No COM port
is opened and no physical motor is run. Checks both accuracy and fret builds.
"""
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
source = (root / "src/tmc2209_fret_test.cpp").read_text(encoding="utf-8")
source = "\n".join(line for line in source.splitlines()
                   if not line.startswith(("#include <Arduino", "#include <avr/", "#include <util/")))
boundary = "  if (!moving && static_cast<int32_t>(millis() - nextMoveAt) >= 0) beginMove();"
# The revised dispatcher also requires explicit permission to start a move.
revised = "  if (moveScheduled && !moving && static_cast<int32_t>(millis() - nextMoveAt) >= 0) {"
target = revised if revised in source else boundary
assert target in source, "Locate the final dispatch boundary before injecting the ISR"
source = source.replace(target, "  interruptAtDispatchBoundary();\n" + target)
prefix = r'''
#include <cstdint>
#include <deque>
#include <vector>
#include <cassert>
#include <iostream>
#define F(x) x
#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define _BV(x) (1U << (x))
#define ISR(x) void x()
#define ATOMIC_RESTORESTATE 0
#define ATOMIC_BLOCK(x) for (bool once=true; once; once=false)
enum {CS10=0, CS11=1, CS12=2, WGM12=3, OCIE1A=1, OCF1A=1, PD2=2};
uint8_t TCCR1A=0, TCCR1B=0, TIMSK1=0, TIFR1=0, PORTD=0;
uint16_t TCNT1=0, OCR1A=0;
uint32_t clockMs=0;
uint32_t millis() { return clockMs; }
uint32_t micros() { return clockMs*1000; }
void delay(unsigned long ms) { clockMs+=ms; }
void delayMicroseconds(unsigned int) {}
void pinMode(int,int) {}
std::vector<int> directions;
void digitalWrite(int pin,int value) { if(pin==3) directions.push_back(value); }
struct SerialMock {
  std::deque<char> input;
  void begin(int) {}
  int available() { return input.size(); }
  char read() { char c=input.front(); input.pop_front(); return c; }
  void feed(const char* s) { while(*s) input.push_back(*s++); }
  template<class T> void print(T) {}
  template<class T> void println(T) {}
  void print(float,int) {}
  void println(float,int) {}
} Serial;
bool injectCompletion=false;
void interruptAtDispatchBoundary();
'''
suffix = r'''
void interruptAtDispatchBoundary() {
  if (injectCompletion) {
    injectCompletion=false;
    TIMER1_COMPA_vect();
  }
}
void finishInInterruptWindow() {
  assert(moving);
  while(pulsesSent < totalPulses-1) TIMER1_COMPA_vect();
  injectCompletion=true;
  // ISR fires AFTER the completion handler and BEFORE the launch check.
  loop();
  assert(!moving && moveFinished); // Must not relaunch with the previous DIR.
  assert(directions.size()==1);
  loop(); // Consume the completion exactly once.
  assert(movesDone==1 && !moving);
}
int main() {
  setup();
#ifdef ACCURACY_TEST
  Serial.feed("hz"); loop();
  assert(referenceReady);
  Serial.feed("hA1,800,30\n"); loop();
#else
  Serial.feed("hM1,800\n"); loop();
#endif
  assert(!moving);
  clockMs+=25; loop();
  assert(moving && directions.size()==1 && directions.back()==HIGH);
  finishInInterruptWindow();
#ifdef ACCURACY_TEST
  assert(awaitingMeasurement);
  for (int n=0; n<5; ++n) { clockMs+=10; loop(); }
  assert(!moving && directions.size()==1); // Held until measurement recorded.
  Serial.feed("hcc"); loop(); // Duplicate acknowledgement cannot reverse twice.
  assert(!moving && !forward);
  for (int index=1; index<60; ++index) {
    heartbeat=clockMs;
    clockMs+=251; loop();
    assert(moving && directions.size()==static_cast<unsigned>(index+1));
    assert(directions.back()==(index%2 ? LOW : HIGH));
    while(moving) TIMER1_COMPA_vect();
    loop();
    assert(awaitingMeasurement && movesDone==index+1);
    Serial.feed("hcc"); loop();
  }
  assert(!session && !moving && directions.size()==60);
  clockMs+=500; loop();
  assert(directions.size()==60);
  // Abort clears any move waiting to start.
  Serial.feed("hzA1,800,1\n"); loop();
  Serial.feed("hx"); loop();
  clockMs+=300; loop();
  assert(!moving && !session && directions.size()==60);
#else
  for (int index=1; index<6; ++index) {
    heartbeat=clockMs;
    clockMs+=400; loop();
    assert(moving && directions.back()==(index%2 ? LOW : HIGH));
    while(moving) TIMER1_COMPA_vect();
    loop();
  }
  assert(!session && directions.size()==6);
  Serial.feed("hM125,100\n"); loop();
  assert(!session && !moving); // Peak demand exceeds 50k STEP/s.
  Serial.feed("hM1,19\n"); loop();
  assert(!session && !moving);
  Serial.feed("hM1,5001\n"); loop();
  assert(!session && !moving);
  const char* commands[]={"hM1,20\n", "hM125,143\n", "hM300,343\n", "hM1,5000\n"};
  for (auto command : commands) {
    Serial.feed(command); loop();
    assert(session && moveScheduled && !moving);
    uint32_t profilePulses=0;
    double profileUs=0;
    for (unsigned i=0; i<segmentCount; ++i) {
      assert(segmentTicks[i]>0);
      const unsigned divisor=segmentClock[i]==_BV(CS12) ? 256
          : segmentClock[i]==(_BV(CS11)|_BV(CS10)) ? 64 : 8;
      assert(segmentTicks[i]*divisor>=320); // At most 50k STEP/s at 16MHz.
      profilePulses+=segmentPulses[i]*2;
      profileUs+=segmentPulses[i]*2.0*segmentTicks[i]*divisor/16;
    }
    assert(profilePulses==totalPulses);
    assert(std::abs(profileUs-durationMs*1000.0)<durationMs*10.0);
    clockMs+=25; loop();
    const auto previousCount=directions.size();
    assert(moving && directions.back()==HIGH);
    while(moving) TIMER1_COMPA_vect();
    loop();
    clockMs+=400; heartbeat=clockMs; loop();
    assert(moving && directions.back()==LOW && directions.size()==previousCount+1);
    Serial.feed("hx"); loop();
    assert(!session && !moving && !moveScheduled);
  }
#endif
  std::cout << "PASS: ISR completion race, alternating DIR, exact move count";
#ifdef ACCURACY_TEST
  std::cout << ", measurement holds, duplicate acknowledgements, abort";
#endif
  std::cout << "\n";
}
'''
directory = root / ".pio/host-sequence-check"
directory.mkdir(parents=True, exist_ok=True)
compiler = shutil.which("g++") or r"C:\Strawberry\c\bin\g++.exe"
for accuracy in (True, False):
    path = directory / ("accuracy.cpp" if accuracy else "fret.cpp")
    binary = path.with_suffix(".exe")
    path.write_text(prefix + source + suffix, encoding="utf-8")
    flags = ["-DACCURACY_TEST"] if accuracy else []
    subprocess.run([compiler, "-std=c++11", "-O2", *flags, str(path), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
