"""Manual single-pulse control. Never connects or actuates automatically."""

import time
import tkinter as tk
from tkinter import ttk

import serial
from serial.tools import list_ports


class Control:
    def __init__(self, root):
        self.root = root
        self.port = None
        self.verified = False
        self.state = "DISCONNECTED"
        self.pending = bytearray()
        self.last_query = 0
        root.title("JF-1253B — одиночный импульс, D6")
        root.geometry("760x500")
        frame = ttk.Frame(root, padding=18)
        frame.pack(fill="both", expand=True)
        ttk.Label(frame, text="Соленоид 24 В · TC4427 + IRLZ44N · вход D6",
                  font=("Segoe UI", 15)).pack(anchor="w")
        ttk.Label(frame, text="Один импульс по нажатию. Предел 100 мс, пауза 5 секунд.").pack(anchor="w", pady=8)
        row = ttk.Frame(frame)
        row.pack(fill="x", pady=8)
        self.ports = ttk.Combobox(row, state="readonly", width=15)
        self.ports.pack(side="left")
        ttk.Button(row, text="Обновить", command=self.scan).pack(side="left", padx=8)
        self.connection = ttk.Button(row, text="Подключить", command=self.connect)
        self.connection.pack(side="left")
        row = ttk.Frame(frame)
        row.pack(fill="x", pady=12)
        ttk.Label(row, text="Длительность, мс:").pack(side="left")
        self.duration = ttk.Combobox(row, state="readonly", width=8, values=(20, 50, 100))
        self.duration.set("50")
        self.duration.pack(side="left", padx=8)
        self.pulse_button = ttk.Button(row, text="Один импульс", command=self.pulse, state="disabled")
        self.pulse_button.pack(side="left", padx=8)
        self.stop_button = ttk.Button(row, text="СТОП / Esc", command=self.stop, state="disabled")
        self.stop_button.pack(side="left")
        self.status = tk.StringVar(value="Сначала загрузите тестовую прошивку, затем подключитесь.")
        ttk.Label(frame, textvariable=self.status, wraplength=710).pack(anchor="w", pady=8)
        self.log = tk.Text(frame, height=10, state="disabled")
        self.log.pack(fill="both", expand=True)
        root.bind("<Escape>", lambda event: self.stop())
        root.protocol("WM_DELETE_WINDOW", self.close)
        self.scan()
        root.after(50, self.poll)

    def scan(self):
        devices = [port.device for port in list_ports.comports()]
        self.ports["values"] = devices
        if self.ports.get() not in devices:
            self.ports.set("COM5" if "COM5" in devices else devices[0] if devices else "")

    def connect(self):
        if self.port:
            self.disconnect()
            return
        try:
            self.port = serial.Serial(self.ports.get(), 115200, timeout=0, write_timeout=0.2)
        except (serial.SerialException, ValueError) as error:
            self.status.set(f"Не удалось подключиться: {error}")
            return
        self.pending.clear()
        self.verified = False
        self.state = "WAIT"
        self.connection.configure(text="Отключить")
        self.status.set("Ожидаем SOLENOID READY V1. Автоматического включения нет.")

    def send(self, command):
        if not self.port or not self.verified:
            return False
        try:
            self.port.write((command + "\n").encode("ascii"))
            return True
        except (serial.SerialException, OSError) as error:
            self.disconnect()
            self.status.set(f"Ошибка связи: {error}")
            return False

    def pulse(self):
        if self.state != "READY":
            return
        if self.send("PULSE " + self.duration.get()):
            self.state = "WAIT"
            self.pulse_button.configure(state="disabled")
            self.status.set("Команда одиночного импульса отправлена.")

    def stop(self):
        if self.send("STOP"):
            self.state = "WAIT"
            self.pulse_button.configure(state="disabled")
            self.status.set("Ожидаем подтверждение остановки.")

    def process_line(self, line):
        if not line.startswith("STATE "):
            self.log.configure(state="normal")
            self.log.insert("end", line + "\n")
            self.log.see("end")
            self.log.configure(state="disabled")
        if line.startswith("SOLENOID READY V1;"):
            self.verified = True
            self.stop_button.configure(state="normal")
            self.send("STATUS")
        elif self.verified:
            if line == "STATE READY":
                self.state = "READY"
                self.status.set("Выход выключен. Готов к одному импульсу.")
            elif line.startswith(("STATE PULSING ", "OK PULSE ")):
                self.state = "PULSING"
                self.status.set("Импульс включён; Timer1 автоматически выключит выход.")
            elif line.startswith("STATE COOLDOWN "):
                self.state = "COOLDOWN"
                remaining = line.rsplit(" ", 1)[-1]
                self.status.set(f"Выход выключен. Пауза: {remaining} мс.")
            elif line.startswith(("DONE;", "OK STOP;")):
                self.state = "WAIT"
                self.status.set("Выход выключен. Ожидаем готовность контроллера.")
            elif line.startswith("ERR "):
                self.state = "WAIT"
                self.status.set("Контроллер: " + line)
        self.pulse_button.configure(state="normal" if self.verified and self.state == "READY" else "disabled")

    def poll(self):
        try:
            if self.port:
                self.pending.extend(self.port.read(min(self.port.in_waiting, 1024)))
                if len(self.pending) > 4096:
                    raise OSError("слишком длинный ответ")
                while b"\n" in self.pending:
                    raw, _, self.pending = self.pending.partition(b"\n")
                    self.process_line(raw.decode("ascii", errors="replace").strip())
                if self.verified and time.monotonic() - self.last_query >= 0.5:
                    self.last_query = time.monotonic()
                    self.send("STATUS")
        except (serial.SerialException, OSError) as error:
            self.disconnect()
            self.status.set(f"Ошибка связи: {error}")
        self.root.after(50, self.poll)

    def disconnect(self):
        port, self.port = self.port, None
        if port:
            try:
                if self.verified:
                    port.write(b"STOP\n")
            except (serial.SerialException, OSError):
                pass
            finally:
                try:
                    port.close()
                except (serial.SerialException, OSError):
                    pass
        self.verified = False
        self.state = "DISCONNECTED"
        self.pulse_button.configure(state="disabled")
        self.stop_button.configure(state="disabled")
        self.connection.configure(text="Подключить")
        self.status.set("Отключено. Аппаратный предел импульса остаётся на Uno.")

    def close(self):
        self.disconnect()
        self.root.destroy()


if __name__ == "__main__":
    window = tk.Tk()
    Control(window)
    window.mainloop()
