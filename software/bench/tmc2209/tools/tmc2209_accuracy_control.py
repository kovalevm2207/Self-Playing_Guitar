"""Manual metrology for bounded out/back moves with holding at every endpoint."""
import csv
import json
import math
import re
import time
from datetime import datetime
from pathlib import Path
import tkinter as tk
from tkinter import ttk, messagebox
from tmc2209_speed_control import Control
from accuracy_results import measurement_row, summarize

MEASURE = re.compile(r"MEASURE index=(\d+); cycle=(\d+); side=(OUT|BACK); target_mm=(\d+); pulses=(\d+); elapsed_us=(\d+)")
FIELDS = ["index", "cycle", "side", "target_mm", "measured_mm", "error_mm", "pulses", "elapsed_ms"]


class AccuracyControl(Control):
    def __init__(self, root):
        self.firmware_ready = self.reference_ready = self.busy = False
        self.pending_point = None
        self.rows = []
        self.result_path = self.csv_path = None
        self.parameters = None
        super().__init__(root, calibration=True)
        root.title("TMC2209 — точность перемещения и возврата")
        root.geometry("1050x850")
        self.start_button.configure(text="2. Начать измерения")
        self.speed_text.set("1. Удержать мотор → обнулить прибор → 2. Начать")
        self.log.configure(state="normal")
        self.log.delete("1.0", "end")
        self.log.configure(state="disabled")
        for widget in self.log.master.winfo_children():
            if isinstance(widget, ttk.Label) and "Только свободный вал" in str(widget.cget("text")):
                widget.configure(text="GT2 20 зубьев, 1/8, 40 STEP/мм. Концевики/StallGuard не используются.\n"
                                 "В каждой точке мотор удерживает позицию до записи измерения.\n"
                                 "Вводите реальную координату от начального нуля, а не заданную длину.")
        frame = self.log.master
        config = ttk.Frame(frame)
        config.pack(fill="x", before=self.log, pady=8)
        self.config_widgets = []
        for label, name, default in [("Ход, мм (1–300)", "distance", "125"),
                                     ("Переезд, мс (300–5000)", "duration", "800"),
                                     ("Циклы (1–30)", "cycles", "5"),
                                     ("Допуск, ±мм", "tolerance", "0.5")]:
            ttk.Label(config, text=label).pack(side="left", padx=(0, 4))
            var = tk.StringVar(value=default)
            setattr(self, name, var)
            entry = ttk.Entry(config, textvariable=var, width=7)
            entry.pack(side="left", padx=(0, 12))
            self.config_widgets.append(entry)
        action = ttk.Frame(frame)
        action.pack(fill="x", before=self.log, pady=4)
        self.reference_button = ttk.Button(action, text="1. Удержать мотор для установки нуля", command=self.reference)
        self.reference_button.pack(side="left")
        self.zero_confirmed = tk.BooleanVar(value=False)
        self.zero_checkbox = ttk.Checkbutton(action, text="Прибор обнулён в удерживаемой начальной точке", variable=self.zero_confirmed)
        self.zero_checkbox.pack(side="left", padx=10)
        measure = ttk.Frame(frame)
        measure.pack(fill="x", before=self.log, pady=8)
        ttk.Label(measure, text="Измеренная координата, мм (возврат может быть отрицательным):").pack(side="left")
        self.measured = tk.StringVar()
        self.measured_entry = ttk.Entry(measure, textvariable=self.measured, width=12)
        self.measured_entry.pack(side="left", padx=8)
        self.record_button = ttk.Button(measure, text="Записать и продолжить (Enter)", command=self.record, state="disabled")
        self.record_button.pack(side="left")
        self.table = ttk.Treeview(frame, columns=FIELDS[:6], show="headings", height=6)
        for key, title in zip(FIELDS[:6], ["Точка", "Цикл", "Направление", "Цель, мм", "Измерено, мм", "Ошибка, мм"]):
            self.table.heading(key, text=title)
            self.table.column(key, width=110, anchor="center")
        self.table.pack(fill="x", before=self.log, pady=6)
        self.report_text = tk.StringVar(value="Ошибка = измеренная координата − целевая. До измерений результат неизвестен.")
        ttk.Label(frame, textvariable=self.report_text, wraplength=980).pack(fill="x", before=self.log)
        root.bind("<Return>", lambda event: self.record() if self.pending_point else self.start())
        self.append("Сначала обеспечьте свободный ход и установите измерительный прибор.")
        self.append("После «Удержать» дождитесь фиксации вала, обнулите прибор, отметьте галочку.")
        self.append("Пуск: ход вперёд → ввод координаты → возврат → ввод координаты; повторить циклы.")

    def settings(self):
        distance, duration, cycles = int(self.distance.get()), int(self.duration.get()), int(self.cycles.get())
        tolerance = float(self.tolerance.get().replace(",", "."))
        if not (1 <= distance <= 300 and 300 <= duration <= 5000 and 1 <= cycles <= 30
                and math.isfinite(tolerance) and tolerance > 0):
            raise ValueError("Settings out of range")
        return {"distance_mm": distance, "duration_ms": duration, "cycles": cycles,
                "tolerance_mm": tolerance, "steps_per_mm": 40, "measurement_source": "manual"}

    def connect(self):
        self.firmware_ready = self.reference_ready = False
        super().connect()

    def reference(self):
        if not self.busy and self.firmware_ready and self.port and time.monotonic() >= self.ready_at:
            self.zero_confirmed.set(False)
            self.send(b"hz")

    def set_busy(self, busy):
        self.busy = busy
        if hasattr(self, "config_widgets"):
            for widget in self.config_widgets:
                widget.configure(state="disabled" if busy else "normal")
            self.reference_button.configure(state="disabled" if busy else "normal")
            self.zero_checkbox.configure(state="disabled" if busy else "normal")

    def start(self):
        if self.busy or not self.port or time.monotonic() < self.ready_at:
            return
        if not self.firmware_ready or not self.reference_ready or not self.zero_confirmed.get():
            messagebox.showinfo("Начальный ноль", "Нужна прошивка ACCURACY V2. Нажмите «Удержать», обнулите прибор и подтвердите ноль.")
            return
        try:
            parameters = self.settings()
        except ValueError:
            messagebox.showerror("Настройки", "Ход 1–300 мм, время 300–5000 мс, циклы 1–30, положительный допуск.")
            return
        directory = Path(__file__).resolve().parents[1] / ".pio" / "accuracy-logs"
        try:
            directory.mkdir(parents=True, exist_ok=True)
            stem = datetime.now().strftime("%Y%m%d-%H%M%S-%f")
            self.csv_path, self.result_path = directory / (stem + ".csv"), directory / (stem + ".json")
            with self.csv_path.open("w", encoding="utf-8-sig", newline="") as handle:
                csv.DictWriter(handle, fieldnames=FIELDS).writeheader()
        except OSError as error:
            messagebox.showerror("Журнал", str(error))
            return
        self.parameters = parameters
        self.rows.clear()
        self.pending_point = None
        for item in self.table.get_children(): self.table.delete(item)
        self.save_report(False, "running")
        command = f"hA{parameters['distance_mm']},{parameters['duration_ms']},{parameters['cycles']}\n".encode("ascii")
        if self.send(command):
            self.set_busy(True)
            self.speed_text.set("Переезд. Дождитесь остановки и запроса измерения.")
            self.append(f"CSV: {self.csv_path}")

    def record(self):
        if not self.pending_point or not self.busy:
            return
        try:
            row = measurement_row(self.pending_point, self.measured.get())
        except ValueError:
            messagebox.showerror("Измерение", "Введите измеренную координату числом, например 124.8 или -0.15.")
            return
        try:
            with self.csv_path.open("a", encoding="utf-8-sig", newline="") as handle:
                csv.DictWriter(handle, fieldnames=FIELDS).writerow(row)
        except OSError as error:
            messagebox.showerror("Журнал", str(error))
            return
        self.rows.append(row)
        self.table.insert("", "end", values=[row[k] if k not in ("measured_mm", "error_mm") else f"{row[k]:.3f}" for k in FIELDS[:6]])
        self.pending_point = None
        self.record_button.configure(state="disabled")
        self.measured.set("")
        self.save_report(False, "measuring")
        if self.send(b"hc"):
            self.speed_text.set("Измерение записано. Следующий переезд или завершение.")

    def save_report(self, acknowledged, reason):
        if not self.parameters or not self.result_path:
            return
        report = summarize(self.rows, self.parameters["cycles"], self.parameters["tolerance_mm"], acknowledged)
        try:
            self.result_path.write_text(json.dumps({"parameters": self.parameters, "reason": reason, **report}, ensure_ascii=False, indent=2), encoding="utf-8")
        except OSError as error:
            self.append(f"Ошибка записи отчёта: {error}")
        if report["complete"]:
            verdict = "В допуске" if report["passed"] else "ВНЕ допуска"
            self.report_text.set(f"{verdict}: max |ошибка| {report['max_abs_error_mm']:.3f} мм; "
                                 f"max |возврат| {report['max_abs_return_error_mm']:.3f} мм. "
                                 f"CSV и подробный отчёт: {self.csv_path}")
        else:
            self.report_text.set(f"Тест не завершён: записано {len(self.rows)}/{self.parameters['cycles']*2} точек. Итоговый допуск ещё не подтверждён.")

    def disconnect(self):
        if self.busy: self.save_report(False, "disconnected")
        self.firmware_ready = self.reference_ready = False
        self.pending_point = None
        self.set_busy(False)
        if hasattr(self, "record_button"): self.record_button.configure(state="disabled")
        super().disconnect()

    def process_line(self, text):
        if text.startswith("ACCURACY READY V2;"):
            self.firmware_ready = True
        elif text.startswith("ACCURACY READY"):
            self.firmware_ready = self.reference_ready = False
            self.speed_text.set("Загрузите ACCURACY V2: в старой версии возможен повтор переезда.")
        elif text.startswith("REFERENCE_READY"):
            self.reference_ready = True
            self.speed_text.set("Мотор удерживается. Теперь обнулите прибор и подтвердите ноль.")
        else:
            match = MEASURE.fullmatch(text)
            if match and self.busy:
                index, cycle, side, target, pulses, elapsed = match.groups()
                number = int(index)
                expected_side = "OUT" if number % 2 else "BACK"
                expected_target = self.parameters["distance_mm"] if number % 2 else 0
                valid = (number == len(self.rows) + 1 and self.pending_point is None
                         and number <= self.parameters["cycles"] * 2
                         and int(cycle) == (number + 1) // 2 and side == expected_side
                         and int(target) == expected_target
                         and int(pulses) == self.parameters["distance_mm"] * 40)
                if not valid:
                    self.send(b"x")
                    self.save_report(False, "point_sequence_error")
                    self.reference_ready = False
                    self.zero_confirmed.set(False)
                    self.pending_point = None
                    self.record_button.configure(state="disabled")
                    self.set_busy(False)
                    self.speed_text.set("Ошибка последовательности точек. Тест прерван; установите ноль заново.")
                else:
                    self.pending_point = {"index": int(index), "cycle": int(cycle), "side": side,
                                          "target_mm": float(target), "pulses": int(pulses), "elapsed_ms": int(elapsed)/1000}
                    self.measured.set("")
                    self.record_button.configure(state="normal")
                    self.measured_entry.focus_set()
                    self.speed_text.set(f"Цикл {cycle}, цель {target} мм. Измерьте координату от начального нуля.")
            if text.startswith("DONE_ACCURACY") and self.busy:
                self.set_busy(False)
                self.save_report(True, "completed")
                self.pending_point = None
                self.record_button.configure(state="disabled")
                self.speed_text.set("Серия завершена; результат измерений указан ниже. Мотор удерживает точку.")
            elif text.startswith(("STOPPED", "ERROR", "REFERENCE_LOST", "MEASUREMENT_TIMEOUT", "LINK TIMEOUT", "TIMING ERROR")):
                self.save_report(False, text)
                self.reference_ready = False
                self.zero_confirmed.set(False)
                self.pending_point = None
                self.set_busy(False)
                self.record_button.configure(state="disabled")
                self.speed_text.set("Тест прерван. Восстановите исходную точку и заново установите ноль.")
        self.append(text)

    def tick(self):
        super().tick()
        if self.port and time.monotonic() >= self.ready_at:
            self.status.set("ACCURACY V2 готова; heartbeat удерживает мотор во время измерения (не более 120 с на точку)." if self.firmware_ready
                            else "Ожидание ACCURACY READY V2 — загрузите прошивку точности.")


if __name__ == "__main__":
    root = tk.Tk()
    AccuracyControl(root)
    root.mainloop()
