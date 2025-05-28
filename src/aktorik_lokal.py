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

        
        # Sensorwerte-Rahmen
        sensor_frame = ttk.LabelFrame(root, text="Sensorwerte", padding=10)
        sensor_frame.grid(row=0, column=0, padx=10, pady=10, sticky="nsew")

        self.grössen = ["Drehzahl", "Drehmoment", "Spannung", "Strom"]
        self.labels = {}
        for i, name in enumerate(self.grössen):
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

        self.value_series = [collections.deque(maxlen=100) for _ in range(4)]
        self.timestep = 0
        self.lines = []
        colors = ["tab:blue", "tab:orange", "tab:green", "tab:red"]
        for i, ax in enumerate(self.axes.flatten()):
            ax.set_xlim(0, 100)
            y_limit = [(0,1000), (0,1), (-1,32), (-6,6)][i]
            ax.set_ylim(*y_limit)
            ax.set_title(self.grössen[i])
            ax.set_xlabel("Zeit (t)")
            y_label = ["n/rpm", "M/Nm", "U/V", "I/A"][i]
            ax.set_ylabel(y_label)
            line, = ax.plot([], [], color=colors[i])
            self.lines.append(line)



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
                if not line: 
                    print("serial gives nothing")
                    continue
                
                parts = line.split(",")
                if not len(parts) == 4: 
                    print("serial reading not giving 4 values")
                    continue 
                
                real_values = [parts[0], self.umrechnung_drehmoment(parts[1]), self.umrechnung_spannung(parts[2]), self.umrechnung_strom(parts[3])]


                self.labels[0].config(text=parts[0])
                self.value_series[0].append(float(parts[0]))
                
                self.labels[1].config(text=f"{parts[1]} ({real_values[1]:.03f} Nm)")
                self.value_series[1].append(float(real_values[1]))
                
                self.labels[2].config(text=f"{parts[2]} ({real_values[2]:.03f} V)")
                self.value_series[2].append(float(real_values[2]))
                
                self.labels[3].config(text=f"{parts[3]} ({real_values[3]:.03f} A)")
                self.value_series[3].append(float(real_values[3]))

            except Exception as e :
                raise e

    def update_plot(self):
        for queue, axis, line in zip(self.value_series, self.axes.flatten(), self.lines):
            line.set_data(range(len(queue)), list(queue))
            axis.relim()
            axis.autoscale_view(scaley=True)
        self.canvas.draw()
        self.root.after(200, self.update_plot)
    
    def umrechnung_drehmoment(self, M_scaled):
        """Sensor: 5V entspricht 1Nm"""
        return float(M_scaled)/5
    
    def umrechnung_spannung(self, V_scaled):
        """Spannungsteiler mit 27k und 5k: 30V werden zu 4,6V"""
        return 5/32*float(V_scaled)
    
    def umrechnung_strom(self, I_sym):
        """Spannung über shuntwiderstand wird um Faktor 20 verstärkt und symmetrisch um 2,5V ausgegeben"""
        shunt_widerstand = 20e-3
        verstärkung = 20
        return (float(I_sym)-2.5)/verstärkung/shunt_widerstand

if __name__ == "__main__":
    root = tk.Tk()
    gui = ArduinoGUI(root)
    root.mainloop()
