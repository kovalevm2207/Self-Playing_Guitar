"""Accuracy metrics use manually measured positions, never pulse counts as feedback."""
import math
import statistics


def measurement_row(point, measured):
    measured = float(str(measured).replace(",", "."))
    if not math.isfinite(measured) or abs(measured) > 10000:
        raise ValueError("Measured coordinate must be finite and within +/-10000 mm")
    return {**point, "measured_mm": measured, "error_mm": measured - point["target_mm"]}


def summarize(rows, cycles, tolerance, acknowledged=False):
    complete = acknowledged and len(rows) == cycles * 2
    complete = complete and [r["index"] for r in rows] == list(range(1, cycles * 2 + 1))
    complete = complete and all(r["side"] == ("OUT" if r["index"] % 2 else "BACK") for r in rows)
    out = [r for r in rows if r["side"] == "OUT"]
    back = [r for r in rows if r["side"] == "BACK"]
    worst = max((abs(r["error_mm"]) for r in rows), default=None)
    def spread(points):
        values = [r["measured_mm"] for r in points]
        return max(values) - min(values) if len(values) >= 2 else None
    mean_out = statistics.mean(r["measured_mm"] for r in out) if out else None
    return {
        "complete": bool(complete), "points": len(rows), "expected_points": cycles * 2,
        "tolerance_mm": tolerance, "passed": bool(complete and worst is not None and worst <= tolerance),
        "max_abs_error_mm": worst,
        "mean_out_error_mm": statistics.mean(r["error_mm"] for r in out) if out else None,
        "out_repeatability_range_mm": spread(out), "return_repeatability_range_mm": spread(back),
        "max_abs_return_error_mm": max((abs(r["error_mm"]) for r in back), default=None),
        "last_return_error_mm": back[-1]["error_mm"] if back else None,
        "return_drift_first_to_last_mm": back[-1]["measured_mm"] - back[0]["measured_mm"] if len(back) >= 2 else None,
        "estimated_steps_per_mm": statistics.mean(r["pulses"] for r in out) / mean_out if mean_out and mean_out > 0 else None,
    }
