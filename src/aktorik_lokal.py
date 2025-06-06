import tkinter as tk
from tkinter import ttk
import serial
import threading
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import collections

# Serieller Port deines Arduinos (ggf. anpassen!)
SERIAL_PORT = '/dev/cu.usbmodem11201'
BAUD_RATE = 115200

class ArduinoGUI:
    MESSWERTE = ["Ist-Drehzahl", "Soll-Drehzahl", "Drehmoment", "Zwischenkreisspannung", "Soll-Motorspannung", "PWM", "Ankerstrom"]
    DIAGRAMME = ["Drehzahl", "Drehmoment", "Spannung", "Strom"]
    
    
    def __init__(self, root):
        self.root = root
        root.title("Arduino Steuerung & Anzeige")

        self.serial_conn = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1) 
        
        # Steuerungs-Rahmen
        control_frame = ttk.LabelFrame(root, text="Ausgangssteuerung", padding=10)
        control_frame.grid(row=0, column=1, padx=10, pady=10, sticky="nsew")

        # PWM
        ttk.Label(control_frame, text="Spannung/V:").grid(row=0, column=0, sticky="w")
        self.pwm = tk.DoubleVar()
        self.pwm_scale = ttk.Scale(control_frame, from_=-30.0, to=30.0, variable=self.pwm,
                                    orient="horizontal", command=self.send_pwm, length=400)
        self.pwm_scale.grid(row=0, column=1, columnspan=3, padx=5)
        self.spannung_label = ttk.Label(control_frame, text="0.00 V")
        self.spannung_label.grid(row=1, column=1, sticky="w")
        
        # Drehzahlregelung
        ttk.Label(control_frame, text="Drehzahl/rpm:").grid(row=2, column=0, sticky="w")
        self.drehzahl = tk.IntVar()
        self.drehzahl_scale = ttk.Scale(control_frame, from_=0, to=1000, variable=self.drehzahl,
                                    orient="horizontal", command=self.send_drehzahl, length=400)
        self.drehzahl_scale.grid(row=2, column=1, columnspan=3, padx=5)
        self.drehzahl_label = ttk.Label(control_frame, text="0.00 rpm")
        self.drehzahl_label.grid(row=3, column=1, sticky="w")

        
        # Disable Button
        self.disable_state = True
        self.disable_button = ttk.Button(control_frame, text="Disable: ON", command=self.toggle_disable)
        self.disable_button.grid(row=4, column=0, columnspan=1, pady=(15, 0))

        # Kp Eingabe
        tk.Label(control_frame, text="Kp:").grid(row=4, column=1)
        self.entry_kp = tk.Entry(control_frame)
        self.entry_kp.grid(row=4, column=2)
        btn_kp = tk.Button(control_frame, text="Senden", command=self.send_Kp)
        btn_kp.grid(row=4, column=3)
        
        # Ki Eingabe
        tk.Label(control_frame, text="Ki:").grid(row=5, column=1, padx=10, pady=5)
        self.entry_ki = tk.Entry(control_frame)
        self.entry_ki.grid(row=5, column=2, padx=10, pady=5)
        btn_ki = tk.Button(control_frame, text="Senden", command=self.send_Ki)
        btn_ki.grid(row=5, column=3, padx=10, pady=5)
        
        # Sensorwerte-Rahmen
        sensor_frame = ttk.LabelFrame(root, text="Sensorwerte", padding=10)
        sensor_frame.grid(row=0, column=0, padx=10, pady=10, sticky="nsew")

        self.labels = {}
        for i, name in enumerate(self.MESSWERTE):
            ttk.Label(sensor_frame, text=name + ":").grid(row=i, column=0, sticky="w", pady=5)
            self.labels[i] = ttk.Label(sensor_frame, text="---")
            self.labels[i].grid(row=i, column=1, sticky="w", pady=5)


        plt.rcParams.update({
            'font.size': 3,
            'axes.titlesize': 6,
            'axes.labelsize': 6,
            'xtick.labelsize': 6,
            'ytick.labelsize': 6,
            'legend.fontsize': 6,
        })
        
        # Rahmen für Plot
        plot_frame = ttk.LabelFrame(root, text="Messkurven", padding=10)
        plot_frame.grid(row=1, column=0, columnspan=2, padx=10, pady=10, sticky="nsew")

        self.fig, self.axes = plt.subplots(2, 2, figsize=(4,3), constrained_layout=True)
        # self.fig.tight_layout()
        self.canvas = FigureCanvasTkAgg(self.fig, master=plot_frame)
        self.canvas.get_tk_widget().pack(fill="both", expand=False)

        self.value_series = [collections.deque(maxlen=100) for _ in range(7)]
        self.timestep = 0
        self.lines = []
        
        colors = ["tab:blue", "tab:orange", "tab:green", "tab:red", "tab:purple", "tab:brown"]

        # Anzahl der Linien pro Plot, z.B. [2, 1, 3, 1]
        lines_per_plot = [2, 1, 3, 1]
        series_counter = 0
        for i, ax in enumerate(self.axes.flatten()):
            ax.set_xlim(0, 100)
            y_limit = [(0,1000), (0,1), (-1,32), (-6,6)][i]
            ax.set_ylim(*y_limit)
            ax.set_title(self.DIAGRAMME[i])
            ax.set_xlabel("Zeit (t)")
            y_label = ["n/rpm", "M/Nm", "U/V", "I/A"][i]
            ax.set_ylabel(y_label)
            line, = ax.plot([], [], color=colors[i])
            
            # self.lines.append(line)
            # Mehrere Linien pro Diagramm
            subplot_lines = []
            for j in range(lines_per_plot[i]):
                line, = ax.plot([], [], color=colors[series_counter % len(colors)], label=self.MESSWERTE[series_counter])
                subplot_lines.append(line)
                series_counter += 1
            self.lines.append(subplot_lines)



        # Starte das regelmäßige Plot-Update
        self.update_plot()

        # Starte Thread zum Lesen serieller Daten
        self.read_thread = threading.Thread(target=self.read_serial, daemon=True)
        self.read_thread.start()

    def send_pwm(self, val):
        spannung = float(val)
        self.spannung_label.config(text=f"{spannung:.2f} V")
        self.serial_conn.write(f"U:{spannung:.5f}\n".encode())
        print(f"sendingU; {spannung:.2f}")
        
    def send_drehzahl(self, val):
        drehzahl = int(float(val))
        self.drehzahl_label.config(text=f"{drehzahl} rpm")
        self.serial_conn.write(f"N:{drehzahl}\n".encode())
        print(f"sendingN:{drehzahl}")
        
    def send_Kp(self):
        kp = float(self.entry_kp.get())
        self.serial_conn.write(f"Kp:{kp:.4f}\n".encode())
        print(f"sendingKp:{kp}")
    def send_Ki(self):
        ki = float(self.entry_ki.get())
        self.serial_conn.write(f"Ki:{ki:.4f}\n".encode())
        print(f"sendingKi:{ki}")


    def toggle_disable(self):
        self.disable_state = not self.disable_state
        state_str = "ON" if self.disable_state else "OFF"
        self.serial_conn.write(f"DISABLE:{int(self.disable_state)}\n".encode())
        self.disable_button.config(text=f"Disable: {state_str}")

    def read_serial(self):
        while True:
            try:
                line = self.serial_conn.readline().decode().strip()
                if not line: 
                    print("serial gives nothing")
                    continue
                if len(line) < 2 or line[0] != "<" or line[-1] != ">":
                    print(f"bad arduino signals: {repr(line)=}")
                    continue
                
                parts = line[1:-1].split(",")
                if not len(parts) == 7: 
                    print("serial reading not giving 7 values")
                    continue 
                print(f"line: {repr(line)}")
                drehzahlIst, drehzahlSoll, drehmomentIst, ZWKspannungIst, MotorspannungSoll, PWM, Ankerstrom = parts

                self.labels[0].config(text=drehzahlIst)
                self.value_series[0].append(float(drehzahlIst))
                self.labels[1].config(text=drehzahlSoll)
                self.value_series[1].append(float(drehzahlSoll))
                
                self.labels[2].config(text=f"{drehmomentIst} Nm")
                self.value_series[2].append(float(drehmomentIst))
                
                self.labels[3].config(text=f"{ZWKspannungIst} V")
                self.value_series[3].append(float(ZWKspannungIst))
                
                self.labels[4].config(text=f"{MotorspannungSoll} V")
                self.value_series[4].append(float(MotorspannungSoll))
                self.labels[5].config(text=f"{PWM} %")
                self.value_series[5].append(float(PWM))
                
                self.labels[6].config(text=f"{Ankerstrom} A")
                self.value_series[6].append(float(Ankerstrom))
                

            except Exception as e :
                raise e

    def update_plot(self):
        series_index = 0
        for i, subplot_lines in enumerate(self.lines):
            for j, line in enumerate(subplot_lines):
                values = list(self.value_series[series_index])
                line.set_data(range(len(values)), values)
                series_index += 1
        for axis in self.axes.flatten():
            axis.relim()
            axis.autoscale_view(scaley=True)
        self.canvas.draw()
        self.root.after(200, self.update_plot)
            
    
if __name__ == "__main__":
    root = tk.Tk()
    gui = ArduinoGUI(root)
    root.mainloop()
