"""Capture the boot scan of the read-only UART test; no motion commands."""
from pathlib import Path
import time
import serial
import argparse

parser = argparse.ArgumentParser()
parser.add_argument("--query", action="store_true", help="Send only '?' after boot; never start motion")
args = parser.parse_args()
lines = []
with serial.Serial("COM5", 115200, timeout=0.2) as port:
    began = time.monotonic()
    end = began + 12
    query_sent = False
    while time.monotonic() < end:
        if args.query and not query_sent and time.monotonic() - began > 3:
            port.write(b"?")
            query_sent = True
        raw = port.readline()
        if raw:
            line = raw.decode("ascii", errors="replace").rstrip()
            lines.append(line)
            print(line, flush=True)
output = Path(__file__).resolve().parents[1] / ".pio" / ("uart-stallguard-check.txt" if args.query else "uart-minimal-result.txt")
output.parent.mkdir(exist_ok=True)
output.write_text("\n".join(lines), encoding="utf-8")
print(f"Saved: {output}")
