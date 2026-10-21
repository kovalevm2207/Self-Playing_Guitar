"""Host tests for the actual firmware readiness/invalid-PWM interlock."""
from pathlib import Path
import shutil
import subprocess

root = Path(__file__).resolve().parents[1]
directory = root / ".pio" / "host-readiness-check"
directory.mkdir(parents=True, exist_ok=True)
source = directory / "check.cpp"
binary = directory / "check.exe"
source.write_text(r'''
#include "stallguard_readiness.h"
#include <cassert>
#include <iostream>
int main() {
  StallGuardReadiness r;
  assert(!r.ready(1000) && !r.invalid());
  r.observe(100); assert(!r.ready(1000));
  r.observe(249); assert(!r.ready(299) && r.ready(300));
  r.observe(250); assert(!r.ready(1000) && !r.invalid());
  r.observe(255); assert(!r.invalid());
  r.observe(255); assert(r.invalid() && !r.ready(1000));
  r.observe(100); assert(!r.invalid() && !r.ready(1000));
  r.observe(100); assert(r.ready(1000));
  r.reset(); assert(!r.ready(1000) && !r.invalid());
  for(int i=0;i<1000;++i) r.observe(255);
  assert(r.bad==255 && r.invalid());
  std::cout << "StallGuard readiness/PWM interlock checks OK\n";
}
''', encoding="utf-8")
compiler = shutil.which("g++")
if not compiler:
    raise SystemExit("Host g++ compiler not found")
subprocess.run([compiler, "-std=c++11", "-I", str(root / "src"), str(source), "-o", str(binary)], check=True)
subprocess.run([str(binary)], check=True)
