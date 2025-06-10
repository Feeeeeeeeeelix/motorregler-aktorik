#include <Arduino.h>



int pwm = 0;

// Pinbelegung Motor
const int pinHighA = 7;  // PWM-Ausgang!
const int pinLowA  = 9;  // bleibt LOW
const int pinHighB = 8;  // LOW
const int pinLowB  = 10;  // HIGH
const int pinEnable = 11; // dauerhaft HIGH
const int rpmPin = 18;    //RPM-Pin
const int OPV = A0;          // OPV-Eingang
const int ivPin = A1;
const int torquePin = A2;

//Hilfsvariablen
unsigned long lastTextTime = 0;
unsigned long requestedRPM = 0;

//RPM Variablen
unsigned long rpm = 0;

//Shunt Variablen
float vRef = 5.0;
float shuntR = 0.1;

//RPM Variablen
const unsigned int rpmDivider = 360;
volatile unsigned long pulseCount = 0;
unsigned long lastMeasurementTime = 0;
unsigned long lastPulseCount = 0;

//PID Variablen
float kp = 0.5, ki = 0.1, kd = 0.00;
float integral = 0;
float lastError = 0;


void countPulse() {
  pulseCount++;
}

int computePID(float targetRPM) {
  float error = targetRPM - rpm;
  integral += error;
  float derivative = error - lastError;
  float pidOutput = kp * error + ki * integral + kd * derivative;
  lastError = error;
  
  pidOutput = constrain(pidOutput, -255, 255);
  return (int)pidOutput;
}

float readTorque() {
  float adcValue = analogRead(torquePin);
  float voltage = (adcValue / 1023.0) * vRef;
  float torque = (2.0 * voltage) - 5.0;

  return torque;
}

float readCurrent() {
  float opvValue = analogRead(OPV);
  float voltage = (opvValue / 1023.0) * vRef;
  // Berechnung des Stroms
  float current = (voltage - 2.5) / (3 * shuntR) * 0.9;  // Strom in Ampere
  return current;
}

void updateRPM() {
  unsigned long pulses = pulseCount - lastPulseCount;
  lastPulseCount = pulseCount;
  rpm = (pulses * 60) / rpmDivider;
  lastMeasurementTime = millis();
}
    
float readIV() {
  float iv = analogRead(ivPin);
  iv = iv / 1025 * 30;
  return iv;
}

void prechargeBootstraps() {
  digitalWrite(pinEnable, HIGH);
  digitalWrite(pinHighA, LOW);
  digitalWrite(pinLowA, HIGH);
  digitalWrite(pinHighB, LOW);
  digitalWrite(pinLowB, HIGH);
  delay(5);  // 5 ms Bootstrap-Vorladen
  digitalWrite(pinLowA, LOW);
  digitalWrite(pinLowB, LOW);
}

void setup() {
  Serial.begin(115200);

  // H-Brücken-Pins konfigurieren
  pinMode(pinHighA, OUTPUT);
  pinMode(pinLowA, OUTPUT);
  pinMode(pinHighB, OUTPUT);
  pinMode(pinLowB, OUTPUT);
  pinMode(pinEnable, OUTPUT);

  prechargeBootstraps();


  // Richtung setzen: Vorwärts
  digitalWrite(pinEnable, HIGH);
  digitalWrite(pinHighB, LOW);
  digitalWrite(pinLowA, LOW);
  //digitalWrite(pinHighB, LOW);
  digitalWrite(pinLowB, HIGH);

  analogWrite(pinHighA, LOW);


  attachInterrupt(digitalPinToInterrupt(rpmPin), countPulse, RISING); // TTL-Signal an Pin 2
  lastMeasurementTime = millis();
  lastTextTime = millis();

}

void loop() {

updateRPM();

  if (millis() - lastTextTime >= 1000) { // alle 1000 ms (1 Sekunde)
    
    Serial.print("RPM: ");
    Serial.println(rpm);

    Serial.print("Strom: ");
    Serial.println(readCurrent());

    Serial.print("Zwischenspannung: ");
    Serial.println(readIV());

    Serial.print("Drehmoment: ");
    Serial.println(readTorque());

    lastTextTime = millis();
  }





}
