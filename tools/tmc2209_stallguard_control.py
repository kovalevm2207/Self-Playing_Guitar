"""Tuning window for the bounded StallGuard4 test (SG firmware V2)."""
import time
import re
from pathlib import Path
from datetime import datetime
import tkinter as tk
from tkinter import ttk, messagebox
from tmc2209_speed_control import Control


class StallGuardControl(Control):
    def __init__(self, root):
        self.firmware_ready = False
        self.busy = False
        self.log_file = None
        self.last_sample_at = 0
        self.cruise_values = []
        super().__init__(root, calibration=True)
        root.title("TMC2209 — StallGuard4: определение нагрузки")
        root.geometry("980x720")
        self.speed_text.set("Настройте UART и DIAG. Порог не является измерением силы.")
        self.log.configure(state="normal")
        self.log.delete("1.0", "end")
        self.log.configure(state="disabled")
        for widget in self.log.master.winfo_children():
            if isinstance(widget, ttk.Label) and "Только свободный вал" in str(widget.cget("text")):
                widget.configure(text="Дополнительно: DIAG → D5; UART RX D8; TX D9 через 1 кОм к тому же PDN.\n"
                                 "MS1/MS2/MS3 OFF. Микрошаг 1/8, ток задаётся вашим VREF.\n"
                                 "До ARMED определение нагрузки отключено. Концевик не заменяется.")
        self.fields = []
        row = ttk.Frame(self.log.master)
        row.pack(fill="x", before=self.log, pady=8)
        for label, name, default in [("Обороты (60–240):", "rpm", "120"),
                                     ("Порог SGTHRS (0–255):", "threshold", "80"),
                                     ("Предел хода, мм (1–125):", "distance", "125")]:
            ttk.Label(row, text=label).pack(side="left", padx=(0, 4))
            value = tk.StringVar(value=default)
            setattr(self, name, value)
            entry = ttk.Entry(row, textvariable=value, width=6)
            entry.pack(side="left", padx=(0, 12))
            self.fields.append(entry)
        row2 = ttk.Frame(self.log.master)
        row2.pack(fill="x", before=self.log, pady=4)
        ttk.Label(row2, text="Направление:").pack(side="left")
        self.direction = ttk.Combobox(row2, state="readonly", values=["Вперёд", "Назад"], width=12)
        self.direction.current(0)
        self.direction.pack(side="left", padx=8)
        ttk.Button(row2, text="Проверить UART", command=self.check_uart).pack(side="left", padx=8)
        ttk.Button(row2, text="Отметить нагрузку в логе", command=lambda: self.send(b"m") if self.busy else None).pack(side="left", padx=8)
        mode_row = ttk.Frame(self.log.master)
        mode_row.pack(fill="x", before=self.log, pady=4)
        self.observe = tk.BooleanVar(value=False)
        self.observe_checkbox = ttk.Checkbutton(mode_row, variable=self.observe,
            text="Наблюдение БЕЗ остановки по SG/DIAG — только стенд с упругой нагрузкой")
        self.observe_checkbox.pack(anchor="w")
        self.append("Начните без препятствия. Один запуск — один проход до предела хода, DIAG или 5 секунд.")
        self.append("Больше SGTHRS → чувствительнее; ниже SG_RESULT → обычно выше нагрузка.")
        self.append("Наращивайте нагрузку через упругий элемент. Не удерживайте вращающийся вал рукой.")
        self.append("После DIAG_STOP положение неизвестно; возврат автоматически не выполняется.")

    def append(self, text):
        super().append(text)
        if getattr(self, "log_file", None):
            try:
                self.log_file.write(f"{datetime.now().isoformat(timespec='milliseconds')} {text}\n")
                self.log_file.flush()
            except OSError:
                self.log_file.close()
                self.log_file = None

    def connect(self):
        self.firmware_ready = False
        super().connect()

    def set_busy(self, busy):
        self.busy = busy
        if hasattr(self, "fields"):
            for field in self.fields:
                field.configure(state="disabled" if busy else "normal")
            self.direction.configure(state="disabled" if busy else "readonly")
            if hasattr(self, "observe_checkbox"):
                self.observe_checkbox.configure(state="disabled" if busy else "normal")

    def disconnect(self):
        self.firmware_ready = False
        self.set_busy(False)
        if self.log_file:
            self.log_file.close()
            self.log_file = None
        super().disconnect()

    def check_uart(self):
        if self.port and not self.busy:
            self.send(b"?")

    def settings(self):
        rpm, threshold, distance = int(self.rpm.get()), int(self.threshold.get()), int(self.distance.get())
        if not (60 <= rpm <= 240 and 0 <= threshold <= 255 and 1 <= distance <= 125):
            raise ValueError("Settings out of range")
        return rpm, threshold, distance, int(self.direction.get() == "Вперёд")

    def start(self):
        if not self.port or time.monotonic() < self.ready_at or self.busy:
            return
        if not self.firmware_ready:
            messagebox.showerror("Прошивка", "Загрузите обновлённый tmc2209_stallguard_test и дождитесь SG READY V2.")
            return
        try:
            rpm, threshold, distance, direction = self.settings()
        except ValueError:
            messagebox.showerror("Настройки", "Нужны целые числа: RPM 60–240, SGTHRS 0–255, ход 1–125 мм.")
            return
        mode = "O" if self.observe.get() else "S"
        if self.log_file:
            self.log_file.close()
            self.log_file = None
        try:
            directory = Path(__file__).resolve().parents[1] / ".pio" / "stallguard-logs"
            directory.mkdir(parents=True, exist_ok=True)
            log_path = directory / (datetime.now().strftime("%Y%m%d-%H%M%S-%f") + ".txt")
            self.log_file = log_path.open("w", encoding="utf-8")
        except OSError as error:
            messagebox.showerror("Журнал", str(error))
            return
        self.log.configure(state="normal")
        self.log.delete("1.0", "end")
        self.log.configure(state="disabled")
        self.cruise_values.clear()
        self.last_sample_at = 0
        self.append(f"REQUEST mode={mode}; rpm={rpm}; SGTHRS={threshold}; distance_mm={distance}; direction={direction}")
        self.append(f"LOG_FILE: {log_path}")
        if self.send(f"h{mode}{rpm},{threshold},{distance},{direction}\n".encode("ascii")):
            self.set_busy(True)
            self.speed_text.set("Проверка UART и подготовка; определение нагрузки пока отключено")

    def process_line(self, text):
        if text.startswith("SG READY V2;"):
            self.firmware_ready = True
        elif text.startswith("ARMED"):
            self.speed_text.set("ARMED — наблюдение, остановка по нагрузке ОТКЛЮЧЕНА" if self.observe.get()
                                else "ARMED — остановка по DIAG и двум низким SG включена")
        elif text.startswith("SG_RESULT="):
            match = re.search(r"SG_RESULT=(\d+); trip_below=(\d+).*; armed=(\d)", text)
            if match:
                self.last_sample_at = time.monotonic()
                result, level, armed = match.groups()
                if armed == "1":
                    self.cruise_values.append(int(result))
                span = (f" · min/max {min(self.cruise_values)}/{max(self.cruise_values)}"
                        if self.cruise_values else "")
                self.speed_text.set(f"SG_RESULT {result} · порог сравнения {level} · "
                                    + (("НАБЛЮДЕНИЕ" if self.observe.get() else "ARMED")
                                       if armed == "1" else "РАЗГОН") + span)
        if text.startswith(("STOPPED", "DIAG_STOP", "SG_SOFTWARE_STOP", "DONE_", "TIME_LIMIT", "LINK_TIMEOUT", "UART_LOST", "DRIVER_FAULT", "MODE_ERROR", "ERROR")):
            self.set_busy(False)
            self.speed_text.set(text)
        self.append(text)

    def tick(self):
        super().tick()
        if self.port and time.monotonic() >= self.ready_at:
            self.status.set("SG V2 готова. Свежие замеры примерно каждые 50 мс; после остановки замеров нет." if self.firmware_ready
                            else "Ожидание SG READY V2 — обновите прошивку.")
            if self.busy and self.last_sample_at and time.monotonic() - self.last_sample_at > 0.5:
                self.status.set("Нет свежего SG_RESULT более 0,5 с — последнее число не является текущим замером.")


if __name__ == "__main__":
    root = tk.Tk()
    StallGuardControl(root)
    root.mainloop()
