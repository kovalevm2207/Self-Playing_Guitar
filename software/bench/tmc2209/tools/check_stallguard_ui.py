"""Exercise paired trial validation and analysis without connecting hardware."""
import sys
from pathlib import Path
import tkinter as tk

sys.path.append(str(Path.home() / ".platformio/penv/Lib/site-packages"))
import tmc2209_stallguard_control as module


class FakePort:
    in_waiting = 0
    def read(self, count): return b""
    def close(self): pass


root = tk.Tk()
root.withdraw()
app = module.StallGuardControl(root)
sent, notices = [], []
module.messagebox.showinfo = lambda *args: notices.append(args)
module.messagebox.showerror = lambda *args: notices.append(args)
app.port = FakePort()
app.ready_at = 0
app.send = lambda data: sent.append(data) or True
assert app.settings() == (60, 80, 125, 1)
app.process_line("SG READY V2; old")
assert not app.firmware_ready
app.process_line("SG READY V3; current")
app.start()
assert not sent and notices  # Load trial cannot run before a baseline.
app.start(baseline=True)
assert sent[-1] == b"hB60,80,125,1\n"
for i in range(12):
    app.process_line(f"SG_RESULT={300+i}; trip_below=160; armed=1; phase=BASELINE; PWM=150")
app.process_line("DONE_LIMIT: no DIAG detected")
assert app.baseline and not app.busy
app.threshold.set("160")
before = len(sent)
app.start()
assert len(sent) == before  # Threshold must not trigger on free motion.
app.threshold.set("80")
app.start()
assert sent[-1] == b"hS60,80,125,1\n"
for sg in (310, 280, 200, 100):
    app.process_line(f"SG_RESULT={sg}; trip_below=160; armed=1; phase=LOAD; PWM=150")
app.process_line("SG_SOFTWARE_STOP: low readings")
assert app.suggested_threshold == 100 and not app.busy
app.apply_suggestion()
assert app.threshold.get() == "100"
app.start(baseline=True)
for i in range(12):
    app.process_line("SG_RESULT=300; trip_below=200; armed=1; phase=BASELINE; PWM=255")
app.process_line("INVALID_PWM_STOP: test")
assert app.baseline is None and "недостоверен" in app.analysis_text.get()
app.port = None
app.close()
print("Paired baseline/load UI, thresholds and invalid-PWM checks OK")
