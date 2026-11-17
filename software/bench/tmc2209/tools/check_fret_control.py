"""Check editable fast FRET settings, protocol version and validation without COM."""
from pathlib import Path
import sys
import tkinter as tk
sys.path.append(str(Path.home() / ".platformio/penv/Lib/site-packages"))
from tmc2209_fret_control import FretControl, minimum_duration, validate_duration

assert minimum_duration(125) == 143
assert minimum_duration(300) == 343
for distance, duration in [(1, 20), (125, 143), (125, 150), (300, 343), (300, 5000)]:
    assert validate_duration(distance, str(duration)) == duration
for distance, duration in [(1, ""), (1, "0"), (1, "19"), (1, "5001"),
                           (125, "142"), (300, "342"), (1, "25.5")]:
    try:
        validate_duration(distance, duration)
    except ValueError:
        pass
    else:
        raise AssertionError((distance, duration))
root = tk.Tk()
root.withdraw()
ui = FretControl(root)
commands = []
ui.send = lambda command: commands.append(command) or True
ui.port = object()
ui.ready_at = 0
ui.process_line("FRET READY V2; test")
assert not ui.firmware_ready
ui.process_line("FRET READY V3; test")
ui.duration.set("150")
assert "5000" in ui.plan_text.get()
ui.start()
assert commands[-1] == b"hM125,150\n" and ui.busy
assert str(ui.duration.cget("state")) == "disabled"
ui.process_line("DONE; test")
assert not ui.busy and str(ui.duration.cget("state")) == "normal"
ui.duration.set("")
assert "143" in ui.plan_text.get()
ui.distance.set("10")
ui.duration.set("20")
ui.start()
assert commands[-1] == b"hM10,20\n"
root.destroy()
print("PASS: FRET V3 handshake, custom times, fast moves and peak-frequency bounds")
