import tkinter as tk
from tkinter import ttk
import serial
import threading
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import collections

# Serieller Port deines Arduinos (ggf. anpassen!)
SERIAL_PORT = '/dev/cu.usbmodem11401'
BAUD_RATE = 9600

class ArduinoGUI:
    def __init__(self, root):
        self.root = root
        root.title("Arduino Steuerung & Anzeige")

        self.serial_conn = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)

        # Sensorwerte-Rahmen
        sensor_frame = ttk.LabelFrame(root, text="Sensorwerte", padding=10)
        sensor_frame.grid(row=0, column=0, padx=10, pady=10, sticky="nsew")

        self.grössen = ["Drehzahl", "Drehmoment", "Spannung", "Strom"]
        self.labels = {}
        for i, name in enumerate(self.grössen):
            ttk.Label(sensor_frame, text=name + ":").grid(row=i, column=0, sticky="w", pady=5)
            self.labels[i] = ttk.Label(sensor_frame, text="---")
            self.labels[i].grid(row=i, column=1, sticky="w", pady=5)

        # Steuerungs-Rahmen
        control_frame = ttk.LabelFrame(root, text="Ausgangssteuerung", padding=10)
        control_frame.grid(row=0, column=1, padx=10, pady=10, sticky="nsew")

        # PWM1
        ttk.Label(control_frame, text="PWM1 [V]:").grid(row=0, column=0, sticky="w")
        self.pwm1 = tk.DoubleVar()
        self.pwm1_scale = ttk.Scale(control_frame, from_=0.0, to=5.0, variable=self.pwm1,
                                    orient="horizontal", command=self.send_pwm1)
        self.pwm1_scale.grid(row=0, column=1, padx=5)
        self.pwm1_label = ttk.Label(control_frame, text="0.00 V")
        self.pwm1_label.grid(row=1, column=1, sticky="w")

        # PWM2
        ttk.Label(control_frame, text="PWM2 [V]:").grid(row=2, column=0, sticky="w", pady=(10, 0))
        self.pwm2 = tk.DoubleVar()
        self.pwm2_scale = ttk.Scale(control_frame, from_=0.0, to=5.0, variable=self.pwm2,
                                    orient="horizontal", command=self.send_pwm2)
        self.pwm2_scale.grid(row=2, column=1, padx=5)
        self.pwm2_label = ttk.Label(control_frame, text="0.00 V")
        self.pwm2_label.grid(row=3, column=1, sticky="w")

        # Disable Button
        self.disable_state = False
        self.disable_button = ttk.Button(control_frame, text="Disable: OFF", command=self.toggle_disable)
        self.disable_button.grid(row=4, column=0, columnspan=2, pady=(15, 0))

        # Rahmen für Plot
        plot_frame = ttk.LabelFrame(root, text="Messkurven", padding=10)
        plot_frame.grid(row=1, column=0, columnspan=2, padx=10, pady=10, sticky="nsew")

        self.fig, self.ax = plt.subplots(figsize=(6, 3))
        self.canvas = FigureCanvasTkAgg(self.fig, master=plot_frame)
        self.canvas.get_tk_widget().pack(fill="both", expand=True)

        self.time_series = [collections.deque(maxlen=100) for _ in range(4)]
        self.timestep = 0

        self.lines = []
        colors = ["tab:blue", "tab:orange", "tab:green", "tab:red"]
        for i, key in enumerate(self.time_series):
            self.lines.append(self.ax.plot([], [], label=key, color=colors[i])[0])

        self.ax.legend(loc="upper right")
        self.ax.set_ylim(0, 5)
        self.ax.set_xlim(0, 100)
        self.ax.set_ylabel("ADC-Wert")
        self.ax.set_xlabel("Zeit (t)")
        self.fig.tight_layout()

        # Starte das regelmäßige Plot-Update
        self.update_plot()

        # Starte Thread zum Lesen serieller Daten
        self.read_thread = threading.Thread(target=self.read_serial, daemon=True)
        self.read_thread.start()

    def send_pwm1(self, val):
        voltage = float(val)
        pwm_val = int((voltage / 5.0) * 255)
        self.pwm1_label.config(text=f"{voltage:.2f} V")
        self.serial_conn.write(f"PWM1:{pwm_val}\n".encode())

    def send_pwm2(self, val):
        voltage = float(val)
        pwm_val = int((voltage / 5.0) * 255)
        self.pwm2_label.config(text=f"{voltage:.2f} V")
        self.serial_conn.write(f"PWM2:{pwm_val}\n".encode())

    def toggle_disable(self):
        self.disable_state = not self.disable_state
        state_str = "ON" if self.disable_state else "OFF"
        self.serial_conn.write(f"DISABLE:{int(self.disable_state)}\n".encode())
        self.disable_button.config(text=f"Disable: {state_str}")

    def read_serial(self):
        while True:
            try:
                line = self.serial_conn.readline().decode().strip()
                if not line: continue
                
                parts = line.split(",")
                if not len(parts) == 4: 
                    print("serial reading not giving 4 values")
                    continue 
                
                for i, value in enumerate(parts):
                    self.labels[i].config(text=value)
                    self.time_series[i].append(float(value))
                self.timestep += 1
            except:
                pass

    def update_plot(self):
        for i, queue in enumerate(self.time_series):
            self.lines[i].set_data(range(len(queue)), list(queue))
        self.ax.relim()
        self.ax.autoscale_view(scaley=True)
        self.canvas.draw()
        self.root.after(50, self.update_plot)

if __name__ == "__main__":
    root = tk.Tk()
    gui = ArduinoGUI(root)
    root.mainloop()
