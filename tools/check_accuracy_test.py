"""Checks manual accuracy metrics and the GUI protocol without opening a COM port."""
import csv
import json
import math
from pathlib import Path
import sys
import tkinter as tk

sys.path.append(str(Path.home() / ".platformio/penv/Lib/site-packages"))
from accuracy_results import measurement_row, summarize
from tmc2209_accuracy_control import AccuracyControl


def point(index, measured):
    return measurement_row({"index": index, "cycle": (index + 1)//2,
                            "side": "OUT" if index % 2 else "BACK",
                            "target_mm": 125 if index % 2 else 0,
                            "pulses": 5000, "elapsed_ms": 800}, measured)


rows = [point(1, "124,8"), point(2, "-0.1"), point(3, "124.9"), point(4, "0.2")]
report = summarize(rows, 2, 0.5, True)
assert report["complete"] and report["passed"]
assert math.isclose(report["max_abs_error_mm"], 0.2)
assert math.isclose(report["out_repeatability_range_mm"], 0.1)
assert math.isclose(report["return_drift_first_to_last_mm"], 0.3)
assert not summarize(rows, 2, 0.05, True)["passed"]
assert not summarize(rows, 2, 0.5, False)["passed"]
assert not summarize(rows[:3], 2, 0.5, True)["complete"]
# Excellent repeatability does not mean accurate travel.
biased = [point(1, 120), point(2, 0), point(3, 120), point(4, 0)]
assert summarize(biased, 2, 0.5, True)["out_repeatability_range_mm"] == 0
assert not summarize(biased, 2, 0.5, True)["passed"]
for invalid in ("nan", "inf", "", "10001"):
    try:
        point(1, invalid)
    except ValueError:
        pass
    else:
        raise AssertionError(invalid)

root = tk.Tk()
root.withdraw()
ui = AccuracyControl(root)
commands = []
ui.send = lambda command: commands.append(command) or True
ui.port = object()  # Never run the serial tick: this is a UI protocol test.
ui.ready_at = 0
ui.process_line("ACCURACY READY V1; test")
assert not ui.firmware_ready
ui.process_line("ACCURACY READY V2; test")
ui.reference()
assert commands[-1] == b"hz"
ui.process_line("REFERENCE_READY; test")
ui.zero_confirmed.set(True)
ui.cycles.set("2")
ui.start()
assert commands[-1] == b"hA125,800,2\n" and ui.busy
for row in rows:
    ui.process_line(f"MEASURE index={row['index']}; cycle={row['cycle']}; side={row['side']}; "
                    f"target_mm={row['target_mm']}; pulses=5000; elapsed_us=800000")
    assert ui.pending_point
    ui.measured.set(str(row["measured_mm"]))
    ui.record()
    assert commands[-1] == b"hc"
ui.process_line("DONE_ACCURACY; all measurements acknowledged; endpoint held")
assert not ui.busy
assert json.loads(ui.result_path.read_text(encoding="utf-8"))["passed"]
with ui.csv_path.open(encoding="utf-8-sig", newline="") as handle:
    assert len(list(csv.DictReader(handle))) == 4
ui.start()
ui.process_line("MEASURE index=1; cycle=1; side=BACK; target_mm=0; pulses=5000; elapsed_us=800000")
assert commands[-1] == b"x" and not ui.busy and not ui.reference_ready
assert not json.loads(ui.result_path.read_text(encoding="utf-8"))["complete"]
root.destroy()
print("PASS: measured accuracy, repeatability, incomplete series, CSV/JSON, GUI handshake and invalid points")
