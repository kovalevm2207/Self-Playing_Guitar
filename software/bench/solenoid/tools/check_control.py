"""Exercise manual controls and the handshake without opening a serial port."""

from pathlib import Path
import sys
import tkinter as tk

sys.path.append(str(Path.home() / ".platformio/penv/Lib/site-packages"))
from control import Control


class FakePort:
    def __init__(self):
        self.sent = []
        self.closed = False

    def write(self, data):
        self.sent.append(data)

    def close(self):
        self.closed = True


root = tk.Tk()
root.withdraw()
ui = Control(root)
port = FakePort()
ui.port = port
ui.process_line("FRET READY V3; wrong firmware")
ui.process_line("STATE READY")
ui.pulse()
assert not port.sent and not ui.verified
assert str(ui.pulse_button.cget("state")) == "disabled"
ui.process_line("SOLENOID READY V1; D6; test")
assert port.sent == [b"STATUS\n"]
ui.process_line("STATE READY")
assert str(ui.pulse_button.cget("state")) == "normal"
ui.pulse()
assert port.sent[-1] == b"PULSE 50\n"
count = len(port.sent)
ui.pulse()
assert len(port.sent) == count
ui.process_line("OK PULSE 50")
ui.process_line("DONE; output=OFF; cooldown_ms=5000")
ui.process_line("STATE COOLDOWN 4990")
assert str(ui.pulse_button.cget("state")) == "disabled"
ui.process_line("STATE READY")
ui.duration.set("20")
ui.pulse()
assert port.sent[-1] == b"PULSE 20\n"
ui.stop()
assert port.sent[-1] == b"STOP\n"
ui.process_line("OK STOP; output=OFF")
assert str(ui.pulse_button.cget("state")) == "disabled"
ui.disconnect()
assert port.closed and port.sent[-1] == b"STOP\n"
assert not ui.verified
root.destroy()
print("PASS: correct firmware handshake, manual pulse only, cooldown, STOP, disconnect")
