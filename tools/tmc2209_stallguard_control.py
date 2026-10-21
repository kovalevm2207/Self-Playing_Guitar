"""Paired baseline/obstacle checks; requires StallGuard firmware V3."""
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
        self.baseline = None
        self.trial_kind = None
        self.trial_signature = None
        self.trial_saturated = False
        super().__init__(root, calibration=True)
        root.title("TMC2209 — StallGuard4: определение нагрузки")
        root.geometry("1050x800")
        self.start_button.configure(text="2. С препятствием (Enter)")
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
        for label, name, default in [("Обороты (60–240):", "rpm", "60"),
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
        self.baseline_button = ttk.Button(mode_row, text="1. Без препятствия — измерить фон",
                                          command=lambda: self.start(baseline=True))
        self.baseline_button.pack(side="left", padx=(0, 8))
        self.apply_threshold_button = ttk.Button(mode_row, text="Применить предложенный порог",
                                                 command=self.apply_suggestion, state="disabled")
        self.apply_threshold_button.pack(side="left")
        self.suggested_threshold = None
        self.analysis_text = tk.StringVar(value="Шаг 1: освободите путь и запишите фон без препятствия.")
        ttk.Label(self.log.master, textvariable=self.analysis_text, wraplength=970).pack(
            fill="x", before=self.log, pady=8)
        self.append("Сначала «1. Без препятствия», затем «2. С препятствием» с теми же настройками.")
        self.append("READY_BASELINE/READY_LOAD появляются только после стабилизации скорости и проверки PWM.")
        self.append("До READY_LOAD не прикладывайте нагрузку. Первый проход должен быть свободным.")
        self.append("Больше SGTHRS → чувствительнее; ниже SG_RESULT → обычно выше нагрузка.")
        self.append("Наращивайте нагрузку через упругий элемент. Не удерживайте вращающийся вал рукой.")
        self.append("После каждого прохода верните каретку в исходную точку вручную. Автовозврата нет.")

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
        self.baseline = None
        super().connect()

    def set_busy(self, busy):
        self.busy = busy
        if hasattr(self, "fields"):
            for field in self.fields:
                field.configure(state="disabled" if busy else "normal")
            self.direction.configure(state="disabled" if busy else "readonly")
            if hasattr(self, "baseline_button"):
                self.baseline_button.configure(state="disabled" if busy else "normal")
                self.apply_threshold_button.configure(state="normal" if not busy and self.suggested_threshold is not None else "disabled")

    def disconnect(self):
        self.firmware_ready = False
        self.baseline = None
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

    def start(self, baseline=False):
        if not self.port or time.monotonic() < self.ready_at or self.busy:
            return
        if not self.firmware_ready:
            messagebox.showerror("Прошивка", "Загрузите обновлённый tmc2209_stallguard_test и дождитесь SG READY V3.")
            return
        try:
            rpm, threshold, distance, direction = self.settings()
        except ValueError:
            messagebox.showerror("Настройки", "Нужны целые числа: RPM 60–240, SGTHRS 0–255, ход 1–125 мм.")
            return
        signature = (rpm, distance, direction)
        if not baseline:
            if not self.baseline or self.baseline["signature"] != signature:
                messagebox.showinfo("Сначала фон", "Выполните «1. Без препятствия» с теми же оборотами, ходом и направлением.")
                return
            if threshold * 2 >= min(self.baseline["values"]) - 10:
                messagebox.showinfo("Слишком высокий порог", "Порог попадает в диапазон свободного хода. Уменьшите SGTHRS; иначе возможна остановка без препятствия.")
                return
        else:
            self.baseline = None
        self.observe.set(baseline)
        self.trial_kind = "BASELINE" if baseline else "LOAD"
        self.trial_signature = signature
        self.trial_saturated = False
        self.suggested_threshold = None
        mode = "B" if baseline else "S"
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
            self.analysis_text.set("Разгон и проверка питания. Дождитесь READY_BASELINE/READY_LOAD.")

    def apply_suggestion(self):
        if self.suggested_threshold is not None and not self.busy:
            self.threshold.set(str(self.suggested_threshold))
            self.analysis_text.set("Пробный порог применён. Повторите проверку без препятствия и с ним; это не гарантия обнаружения.")

    def finish_trial(self, reason):
        if reason.startswith("INVALID_PWM_STOP") or self.trial_saturated:
            self.analysis_text.set("Результат недостоверен: PWM достиг предела. Проверьте VM под нагрузкой, обе обмотки и настройку тока. Порог не подбираем.")
            if self.trial_kind == "BASELINE":
                self.baseline = None
            return
        if self.trial_kind == "BASELINE":
            if not reason.startswith("DONE_LIMIT") or len(self.cruise_values) < 10:
                self.analysis_text.set("Фон не записан: нужен полный свободный проход и минимум 10 замеров после READY_BASELINE.")
                return
            self.baseline = {"signature": self.trial_signature, "values": list(self.cruise_values)}
            low, high = min(self.cruise_values), max(self.cruise_values)
            self.analysis_text.set(f"Фон записан: SG {low}–{high}. Верните каретку в исходную точку; затем шаг 2 с упругим препятствием после участка разгона.")
        elif self.trial_kind == "LOAD" and self.baseline:
            if reason.startswith(("DRIVER_FAULT", "UART_LOST", "MODE_ERROR", "LINK_TIMEOUT", "ERROR")):
                self.analysis_text.set("Тест прерван ошибкой; сравнение нагрузки недостоверно. Смотрите причину в журнале.")
                return
            if reason.startswith("DIAG_STOP"):
                self.analysis_text.set("Есть остановка по DIAG при допустимом фоне. SG мог упасть между замерами; смотрите STOP_LATCH. Повторите с препятствием и без него.")
                return
            if not self.cruise_values:
                self.analysis_text.set("Нет замеров после READY_LOAD — реакция на нагрузку не проверена.")
                return
            baseline_low = min(self.baseline["values"])
            loaded_low = min(self.cruise_values)
            gap = baseline_low - loaded_low
            if gap >= 20:
                self.suggested_threshold = max(0, min(255, (baseline_low + loaded_low) // 4))
                self.apply_threshold_button.configure(state="normal")
                self.analysis_text.set(f"Есть снижение SG: фон min {baseline_low}, с нагрузкой min {loaded_low}. Пробный SGTHRS {self.suggested_threshold}; подтвердите повторными проходами.")
            else:
                self.analysis_text.set(f"Устойчивое различие не подтверждено: фон min {baseline_low}, с нагрузкой min {loaded_low}. Порог наугад не повышаем.")

    def process_line(self, text):
        if text.startswith("SG READY V3;"):
            self.firmware_ready = True
        elif text.startswith("ARMED"):
            self.speed_text.set("Фон — путь свободен, остановка по SG отключена" if self.observe.get()
                                else "ARMED — остановка по DIAG и двум низким SG включена")
        elif text.startswith("READY_BASELINE"):
            self.analysis_text.set("Измеряем фон на постоянной скорости. Не добавляйте нагрузку до конца прохода.")
        elif text.startswith("READY_LOAD"):
            self.analysis_text.set("ГОТОВ К НАГРУЗКЕ: постоянная скорость, PWM прошёл проверку. Можно приложить упругую нагрузку.")
        elif text.startswith("SG_RESULT="):
            match = re.search(r"SG_RESULT=(\d+); trip_below=(\d+).*; armed=(\d)", text)
            if match:
                self.last_sample_at = time.monotonic()
                result, level, armed = match.groups()
                if armed == "1":
                    self.cruise_values.append(int(result))
                    pwm_match = re.search(r"; PWM=(\d+)", text)
                    if pwm_match and int(pwm_match.group(1)) >= 250:
                        self.trial_saturated = True
                span = (f" · min/max {min(self.cruise_values)}/{max(self.cruise_values)}"
                        if self.cruise_values else "")
                self.speed_text.set(f"SG_RESULT {result} · порог сравнения {level} · "
                                    + (("НАБЛЮДЕНИЕ" if self.observe.get() else "ARMED")
                                       if armed == "1" else "РАЗГОН") + span)
        if text.startswith(("STOPPED", "DIAG_STOP", "SG_SOFTWARE_STOP", "INVALID_PWM_STOP", "DONE_", "TIME_LIMIT", "LINK_TIMEOUT", "UART_LOST", "DRIVER_FAULT", "MODE_ERROR", "ERROR")):
            self.set_busy(False)
            self.speed_text.set(text)
            self.finish_trial(text)
        self.append(text)

    def tick(self):
        super().tick()
        if self.port and time.monotonic() >= self.ready_at:
            self.status.set("SG V3: сравнение двух проходов, только после стабилизации и проверки PWM." if self.firmware_ready
                            else "Ожидание SG READY V3 — обновите прошивку.")
            if self.busy and self.last_sample_at and time.monotonic() - self.last_sample_at > 0.5:
                self.status.set("Нет свежего SG_RESULT более 0,5 с — последнее число не является текущим замером.")


if __name__ == "__main__":
    root = tk.Tk()
    StallGuardControl(root)
    root.mainloop()
