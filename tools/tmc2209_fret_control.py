"""Control adjustable-distance motion tests; requires FRET firmware V2."""
import time
import tkinter as tk
from tkinter import ttk, messagebox
from tmc2209_speed_control import Control

DURATIONS = [800, 600, 500, 450, 400, 375, 350, 325, 300]


class FretControl(Control):
    def __init__(self, root):
        self.firmware_ready = False
        self.busy = False
        super().__init__(root, calibration=True)
        root.title("TMC2209 — переезды между ладами, 160 BPM")
        root.geometry("900x680")
        self.speed_text.set("GT2 20 зубьев · микрошаг 1/8 · 40 STEP/мм")
        self.log.configure(state="normal")
        self.log.delete("1.0", "end")
        self.log.configure(state="disabled")
        # Replace the free-shaft warning inherited from the speed controller.
        for widget in self.log.master.winfo_children():
            if isinstance(widget, ttk.Label) and "Только свободный вал" in str(widget.cget("text")):
                widget.configure(text="Начните со свободного вала. С кареткой: обеспечьте выбранную длину хода.\n"
                                 "Нет концевиков и поиска нуля. При срыве — СТОП и восстановление исходной точки.\n"
                                 "После каждого теста проверьте возврат к метке; программа шаги не измеряет.")
        distance_row = ttk.Frame(self.log.master)
        distance_row.pack(fill="x", before=self.log, pady=4)
        ttk.Label(distance_row, text="Длина одного переезда, мм (1–300):").pack(side="left")
        self.distance = tk.StringVar(value="125")
        self.distance_entry = ttk.Entry(distance_row, textvariable=self.distance, width=8)
        self.distance_entry.pack(side="left", padx=8)
        row = ttk.Frame(self.log.master)
        row.pack(fill="x", before=self.log, pady=8)
        ttk.Label(row, text="Время переезда, мс:").pack(side="left")
        self.duration = ttk.Combobox(row, state="readonly", width=8, values=DURATIONS)
        self.duration.set("800")
        self.duration.pack(side="left", padx=8)
        self.duration.bind("<<ComboboxSelected>>", lambda event: self.plan())
        self.plan_text = tk.StringVar()
        ttk.Label(row, textvariable=self.plan_text).pack(side="left")
        self.distance.trace_add("write", lambda *args: self.plan())
        self.plan()
        self.append("Один запуск = 3 переезда туда-обратно на выбранное расстояние. По умолчанию 125 мм.")
        self.append("Начните с 800 мс, затем выбирайте более короткое время после проверки возврата.")
        self.append("160 BPM = 375 мс/четверть; цель 300 мс оставляет 75 мс на установление/зажим.")

    def plan(self):
        try:
            distance = self.get_distance()
        except ValueError:
            self.plan_text.set("Введите целое расстояние от 1 до 300 мм")
            return
        seconds = int(self.duration.get()) / 1000
        peak = distance / (0.7 * seconds)
        self.plan_text.set(f"{distance * 40} имп.; пик {peak * 1.5:.0f} об/мин; ускорение {peak / (0.3 * seconds):.0f} мм/с²")

    def get_distance(self):
        distance = int(self.distance.get())
        if not 1 <= distance <= 300:
            raise ValueError("distance out of range")
        return distance

    def connect(self):
        self.firmware_ready = False
        self.busy = False
        super().connect()

    def start(self):
        if not self.port or time.monotonic() < self.ready_at or self.busy:
            return
        if not self.firmware_ready:
            messagebox.showerror("Прошивка", "Не получено FRET READY V2. Загрузите обновлённый tmc2209_fret_test и переподключитесь.")
            return
        try:
            distance = self.get_distance()
        except ValueError:
            messagebox.showerror("Расстояние", "Введите целое расстояние от 1 до 300 мм.")
            return
        if self.send(f"hM{distance},{self.duration.get()}\n".encode("ascii")):
            self.busy = True
            self.duration.configure(state="disabled")
            self.distance_entry.configure(state="disabled")
            self.speed_text.set(f"Тест: {distance} мм за {self.duration.get()} мс, 3 раза туда-обратно")

    def disconnect(self):
        self.firmware_ready = False
        self.busy = False
        if hasattr(self, "duration"):
            self.duration.configure(state="readonly")
            self.distance_entry.configure(state="normal")
        super().disconnect()

    def process_line(self, text):
        if text.startswith("FRET READY V2;"):
            self.firmware_ready = True
        if text.startswith(("DONE", "STOPPED", "ERROR")):
            self.busy = False
            if hasattr(self, "duration"):
                self.duration.configure(state="readonly")
                self.distance_entry.configure(state="normal")
            self.speed_text.set("Готово — проверьте возврат к метке" if text.startswith("DONE")
                                else "Прервано — восстановите исходную точку перед новым тестом")
        self.append(text)

    def tick(self):
        super().tick()
        if self.port and time.monotonic() >= self.ready_at:
            self.status.set("FRET готов. Выберите время и нажмите ПУСК." if self.firmware_ready
                            else "Ожидание FRET READY V2 — обновите прошивку и переподключитесь.")


if __name__ == "__main__":
    root = tk.Tk()
    FretControl(root)
    root.mainloop()
