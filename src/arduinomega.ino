#include <Arduino.h>
// --- Pinbelegung ---
const int rpmPin = 2;            // Drehzahlgeber (Interrupt-fähig)
const int torquePin = A0;        // Drehmoment (0-5V → ±5V)
const int voltagePin = A1;       // Spannung (0-5V)
const int currentPin = A2;       // Strom (0-5V)

const int pwm1Pin = 5;           // PWM-Ausgang 1
const int pwm2Pin = 6;           // PWM-Ausgang 2
const int disablePin = 7;        // Digitaler Ausgang für Disable

volatile unsigned long pulseCount = 0;
int abtastrate = 500; // ms

void countPulse() {
  pulseCount++;
}

void setup() {
  Serial.begin(9600);
  pinMode(rpmPin, INPUT);
  pinMode(disablePin, OUTPUT);
  pinMode(pwm1Pin, OUTPUT);
  pinMode(pwm2Pin, OUTPUT);

  attachInterrupt(digitalPinToInterrupt(rpmPin), countPulse, RISING);
}

void loop() {

  pulseCount = 0;
  delay(abtastrate);
  unsigned long rpm = (pulseCount * 60UL) / (720 * abtastrate);

  // Sensorwerte lesen
  float torqueV = analogRead(torquePin) * (5.0 / 1023.0);
  float voltage = analogRead(voltagePin) * (5.0 / 1023.0);
  float current = analogRead(currentPin) * (5.0 / 1023.0);

  // Daten senden: "rpm,torqueV,voltage,current"
  Serial.print(rpm);
  Serial.print(",");
  Serial.print(torqueV, 2);
  Serial.print(",");
  Serial.print(voltage, 2);
  Serial.print(",");
  Serial.println(current, 2);

  // Seriell empfangene Steuerbefehle verarbeiten
  while (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    
    if (input.startsWith("PWM1:")) {
      int val = input.substring(5).toInt();
      analogWrite(pwm1Pin, val);

    } else if (input.startsWith("PWM2:")) {
      int val = input.substring(5).toInt();
      analogWrite(pwm2Pin, val);

    } else if (input.startsWith("DISABLE:")) {
      int state = input.substring(8).toInt();
      digitalWrite(disablePin, state);
    }
  }
}