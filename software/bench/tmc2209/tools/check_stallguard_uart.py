"""Host checks of the actual firmware packet functions, without a motor.

Requires a host g++ compiler. This verifies CRC, echo filtering and rejection
of damaged/short/wrong-register replies; it does not validate wire timing.
"""
from pathlib import Path
import shutil
import subprocess

source = (Path(__file__).resolve().parents[1] / "src/tmc2209_stallguard_test.cpp").read_text(encoding="utf-8")
functions = source[source.index("uint8_t crc8("):source.index("// Can be called from either ISR.")]
prefix = r'''
#include <cstdint>
#include <vector>
#include <deque>
#include <cassert>
#include <iostream>
uint32_t clockMs = 0;
uint32_t millis() { return clockMs++; }
void delayMicroseconds(unsigned int) {}
struct MockSerial {
  std::deque<uint8_t> incoming;
  std::vector<uint8_t> reply, lastWrite;
  int available() { return incoming.size(); }
  int read() { auto b=incoming.front(); incoming.pop_front(); return b; }
  void flush() {}
  void write(const uint8_t *bytes, unsigned int count) {
    lastWrite.assign(bytes, bytes+count);
    // The real one-wire bus echoes every byte transmitted by the master.
    incoming.insert(incoming.end(), bytes, bytes+count);
    if (count==4) incoming.insert(incoming.end(), reply.begin(), reply.end());
  }
} driverSerial;
'''
suffix = r'''
int main() {
  const uint8_t gconfRequest[] = {0x05, 0x00, 0x00};
  assert(crc8(gconfRequest, 3) == 0x48);
  writeRegister(0x00, 0x000001C1);
  const auto packet = driverSerial.lastWrite;
  assert(packet.size()==8 && packet[0]==5 && packet[1]==0 && packet[2]==0x80);
  assert(packet[3]==0 && packet[4]==0 && packet[5]==1 && packet[6]==0xC1);
  assert(packet[7]==crc8(packet.data(),7));
  auto makeReply=[](uint8_t reg) {
    std::vector<uint8_t> p={5,0xFF,reg,0x21,0,0,0,0};
    p[7]=crc8(p.data(),7);
    return p;
  };
  uint32_t value=0;
  driverSerial.reply=makeReply(6);
  assert(readRegister(6,value) && value==0x21000000);
  driverSerial.reply[7]^=1;
  assert(!readRegister(6,value));
  driverSerial.reply=makeReply(2);
  assert(!readRegister(6,value));
  driverSerial.reply={5,0xFF,6,0x21};
  assert(!readRegister(6,value));
  driverSerial.reply.clear();
  assert(!readRegister(6,value));
  driverSerial.reply=makeReply(6);
  driverSerial.reply.insert(driverSerial.reply.begin(),{0xAA,0x55});
  assert(readRegister(6,value) && value==0x21000000);
  std::cout << "UART packet checks OK\n";
}
'''
compiler = shutil.which("g++")
if compiler is None:
    raise SystemExit("Host g++ compiler not found")
directory = Path(__file__).resolve().parents[1] / ".pio" / "host-uart-check"
directory.mkdir(parents=True, exist_ok=True)
cpp = directory / "check.cpp"
binary = directory / "check.exe"
cpp.write_text(prefix + functions + suffix, encoding="utf-8")
subprocess.run([compiler, "-std=c++11", "-O2", str(cpp), "-o", str(binary)], check=True)
subprocess.run([str(binary)], check=True)
