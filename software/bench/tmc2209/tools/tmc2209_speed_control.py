"""Desktop buttons for tmc2209_speed_test firmware. Requires pyserial."""
import time
import re
import sys
import tkinter as tk
from tkinter import ttk, messagebox

import serial
from serial.tools import list_ports


def pulses_to_rpm(pulses_per_second, microsteps):
    return pulses_per_second * 60.0 / (200 * microsteps)


class Control:
    def __init__(self, root, calibration=False):
        self.calibration = calibration
        self.root = root
        self.port = None
        self.ready_at = 0
        self.last_heartbeat = 0
        self.pending = bytearray()
        self.last_rate = None
        self.last_target = None
        root.title("TMC2209 — проверка оборота: 3200 импульсов" if calibration else "TMC2209 — тест скорости")
        root.geometry("820x600")
        frame = ttk.Frame(root, padding=12)
        frame.pack(fill="both", expand=True)
        ttk.Label(frame, text="STEP D2 · DIR D3 · EN D4 · 115200 бод").pack(anchor="w")
        row = ttk.Frame(frame)
        row.pack(fill="x", pady=10)
        self.ports = ttk.Combobox(row, width=16, state="readonly")
        self.ports.pack(side="left")
        ttk.Button(row, text="Обновить порты", command=self.scan).pack(side="left", padx=6)
        self.connect_button = ttk.Button(row, text="Подключить", command=self.connect)
        self.connect_button.pack(side="left")
        settings = ttk.Frame(frame)
        settings.pack(fill="x", pady=4)
        ttk.Label(settings, text="Микрошаг драйвера (мотор 1,8°): 1/").pack(side="left")
        self.microsteps = ttk.Combobox(settings, width=8, state="readonly",
                                      values=[1, 2, 4, 8, 16, 32, 64, 128, 256])
        self.microsteps.pack(side="left", padx=6)
        self.microsteps.bind("<<ComboboxSelected>>", lambda event: self.update_speed())
        if calibration:
            settings.pack_forget()
        self.speed_text = tk.StringVar(value="Скорость: выберите микрошаг драйвера")
        ttk.Label(frame, textvariable=self.speed_text, font=("Segoe UI", 16)).pack(anchor="w", pady=8)
        self.status = tk.StringVar(value="Подключите Arduino после загрузки прошивки.")
        ttk.Label(frame, textvariable=self.status).pack(anchor="w")
        buttons = ttk.Frame(frame)
        buttons.pack(fill="x", pady=12)
        self.start_button = ttk.Button(buttons, text="ПУСК (Enter)", command=self.start, state="disabled")
        self.start_button.pack(side="left", ipadx=20, ipady=10)
        ttk.Button(buttons, text="СТОП (Пробел / Esc)", command=self.stop).pack(side="left", padx=12, ipadx=20, ipady=10)
        ttk.Label(frame, text="Только свободный вал. При вибрации или остановке вала нажмите СТОП.\n"
                  "Об/мин рассчитаны по импульсам и микрошагу, без измерения скорости вала.\n"
                  "СТОП отключает драйвер: вал может двигаться по инерции.").pack(anchor="w")
        self.log = tk.Text(frame, height=13, state="disabled")
        self.log.pack(fill="both", expand=True, pady=10)
        if calibration:
            self.speed_text.set("Поставьте метку. Пуск: 3200 импульсов за ≈16 секунд.")
            self.append("Считайте обороты вала от начала до конца теста.")
            self.append("2 оборота → 1/8; 1 оборот → 1/16; ½ → 1/32; ¼ → 1/64 (для мотора 1,8°).")
            self.append("Дождитесь CALIBRATION DONE. Прерванный тест не используйте для расчёта.")
        root.bind("<Return>", lambda event: self.start())
        root.bind("<space>", lambda event: self.stop())
        root.bind("<Escape>", lambda event: self.stop())
        root.protocol("WM_DELETE_WINDOW", self.close)
        self.scan()
        self.tick()

    def update_speed(self):
        if self.calibration:
            return
        if not self.microsteps.get():
            self.speed_text.set("Скорость: выберите микрошаг драйвера")
            return
        if self.last_rate is None:
            self.speed_text.set("Скорость: ожидание данных Arduino")
            return
        microsteps = int(self.microsteps.get())
        rpm = pulses_to_rpm(self.last_rate, microsteps)
        target_rpm = pulses_to_rpm(self.last_target, microsteps)
        self.speed_text.set(f"{rpm:.1f} об/мин  →  цель {target_rpm:.1f} об/мин")

    def process_line(self, text):
        if self.calibration:
            if text.startswith("CALIBRATION pulses="):
                self.speed_text.set(text.replace("CALIBRATION pulses=", "Подано импульсов: "))
            elif text.startswith("CALIBRATION DONE"):
                self.speed_text.set("Готово: 3200 импульсов. Сколько оборотов сделал вал?")
            elif text.startswith("STOPPED"):
                self.speed_text.set("Тест прерван. Повторите от новой метки.")
            self.append(text)
            return
        match = re.fullmatch(r"pulse_rate=(\d+) pulses/s; target=(\d+)", text)
        if match:
            self.last_rate, self.last_target = map(int, match.groups())
            self.update_speed()
            if self.microsteps.get():
                microsteps = int(self.microsteps.get())
                rpm = pulses_to_rpm(self.last_rate, microsteps)
                target_rpm = pulses_to_rpm(self.last_target, microsteps)
                self.append(f"{rpm:.1f} об/мин; цель {target_rpm:.1f} об/мин; "
                            f"STEP={self.last_rate} имп/с; микрошаг 1/{microsteps}")
            else:
                self.append(text + " — выберите микрошаг для пересчёта в об/мин")
        else:
            if text.startswith("STOPPED"):
                self.speed_text.set("Импульсы остановлены — вал может двигаться по инерции")
                self.last_rate = self.last_target = None
            self.append(text)

    def scan(self):
        ports = sorted(list_ports.comports(), key=lambda p: (p.vid is None, p.device))
        devices = [p.device for p in ports]
        self.ports["values"] = devices
        if devices and self.ports.get() not in devices:
            self.ports.set(devices[0])

    def append(self, text):
        self.log.configure(state="normal")
        self.log.insert("end", text + "\n")
        if int(self.log.index("end-1c").split(".")[0]) > 300:
            self.log.delete("1.0", "50.0")
        self.log.see("end")
        self.log.configure(state="disabled")

    def connect(self):
        if self.port:
            self.stop()
            self.disconnect()
            return
        try:
            self.port = serial.Serial(self.ports.get(), 115200, timeout=0, write_timeout=0.2)
            self.pending.clear()
            self.last_rate = self.last_target = None
            self.update_speed()
            self.ready_at = time.monotonic() + 3
            self.connect_button.configure(text="Отключить")
            self.status.set("Подключено. Ожидание перезапуска Arduino…")
        except (serial.SerialException, OSError, ValueError) as error:
            self.port = None
            messagebox.showerror("Подключение", str(error))

    def send(self, command):
        if not self.port:
            return False
        try:
            self.port.write(command)
            return True
        except (serial.SerialException, OSError) as error:
            self.append(str(error))
            self.disconnect()
            return False

    def start(self):
        if self.port and time.monotonic() >= self.ready_at:
            if not self.calibration and not self.microsteps.get():
                messagebox.showinfo("Микрошаг", "Выберите микрошаг, настроенный на драйвере.\n"
                                    "Это значение нужно только для пересчёта об/мин; драйвер оно не настраивает.")
                return
            self.send(b"hs")

    def stop(self):
        self.send(b"x")

    def disconnect(self):
        if self.port:
            try:
                self.port.close()
            except (serial.SerialException, OSError):
                pass
        self.port = None
        self.last_rate = self.last_target = None
        self.speed_text.set("Нет связи — скорость вала неизвестна")
        self.start_button.configure(state="disabled")
        self.connect_button.configure(text="Подключить")
        self.status.set("Отключено. При потере связи прошивка останавливается за 2 с.")

    def tick(self):
        try:
            if self.port:
                now = time.monotonic()
                if now >= self.ready_at:
                    self.start_button.configure(state="normal")
                    self.status.set("Готово. Пуск — ровно 3200 импульсов на малой скорости." if self.calibration
                                    else "Разгон 500 имп/с²; ступень ≥200 имп/с; предел 100 000 имп/с.")
                    if now - self.last_heartbeat >= 0.25:
                        self.last_heartbeat = now
                        self.send(b"h")
                if self.port:
                    self.pending.extend(self.port.read(min(self.port.in_waiting, 4096)))
                    while b"\n" in self.pending:
                        line, _, rest = self.pending.partition(b"\n")
                        self.pending = bytearray(rest)
                        self.process_line(line.decode("ascii", errors="replace").strip())
        except (serial.SerialException, OSError) as error:
            self.append(str(error))
            self.disconnect()
        self.root.after(50, self.tick)

    def close(self):
        self.stop()
        self.disconnect()
        self.root.destroy()


if __name__ == "__main__":
    window = tk.Tk()
    Control(window, calibration="calibration" in sys.argv)
    window.mainloop()
