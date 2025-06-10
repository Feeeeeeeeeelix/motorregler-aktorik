#include <Arduino.h>

const int drehzahlPin = 18;
const int drehmomentPin = A2; //to implement
const int spannungPin = A1; 
const int stromPin = A0;

const int pinHighA = 7; 
const int pinLowA  = 9;  
const int pinHighB = 8; 
const int pinLowB  = 10;

const int pinEnable = 11; 

long DrehZahlIstRPM;
float DrehmomentIst;
float ZwischenkreisSpannungIst;
float AnkerStromIst;

volatile unsigned long pulseCount = 0;
int abtastPeriode = 500; // ms

float MotorSpannungSollV;
bool MotorMode = 0;   // 1: geregelt, 0: manuell über Spannung
int DrehZahlSollRPM = 0;
float PWM;
int frequenz = 2000;
int duty_time;
float MaxPWM = 0.9;

float NReglerKp = 0;
float NReglerKi = 0;
float IReglerKp = 0;
float IReglerKi = 0;

float dN = 0;
float dNsum = 0;

float dI = 0;
float dIsum = 0;

float cephi = 1.0;


void setMotorVoltage(float sollSpannung){
  // U zwischen -30V und +30V. Dazwischen PWM

  // PWM aus Sollmotorspannung und istZwischenkreisfrequenz mit PWM begrenzung auf 90% wegen dem Bootstrap vorm Gatetreiber
  MotorSpannungSollV = sollSpannung;
  PWM = abs(MotorSpannungSollV)/ZwischenkreisSpannungIst;
  // PWM = MotorSpannungSollV/30.0; // als test wenn keine 30V anliegen

  if (PWM > MaxPWM || PWM < -MaxPWM){
    PWM = max(min(PWM, MaxPWM), -MaxPWM);
  }
  
  if (PWM > 0){
    digitalWrite(pinEnable, HIGH);
    digitalWrite(pinHighB, LOW);
    digitalWrite(pinLowB, HIGH);
    digitalWrite(pinLowA, LOW);
    analogWrite(pinHighA, (int)(PWM*256));
  } else if (PWM < 0){
    digitalWrite(pinEnable, HIGH);
    digitalWrite(pinHighA, LOW);
    digitalWrite(pinLowA, HIGH);
    digitalWrite(pinLowB, LOW);
    analogWrite(pinHighB, (int)(-PWM*256));
  } else {
    digitalWrite(pinEnable, LOW);
    digitalWrite(pinHighB, LOW);
    digitalWrite(pinLowB, LOW);
    digitalWrite(pinLowA, LOW);
    analogWrite(pinHighA, LOW);
  }
}

void controlMotorAmp(float sollStrom){
  dI = sollStrom-AnkerStromIst;
  dIsum += dI * abtastPeriode/1000.0;

  double U = dI*IReglerKp + dIsum*IReglerKi;
  setMotorVoltage(min(max(U, -30.0), 30.0));

  if (U > 30.0 || U < -30.0){
    dIsum -= dI * abtastPeriode/1000.0;
  }
}

void controlMotorSpeed(){
  double dN = DrehZahlSollRPM-DrehZahlIstRPM;
  dNsum += dN * abtastPeriode/1000.0;

  double I = (dN*NReglerKp + dNsum*NReglerKi) * cephi;
  controlMotorAmp(min(max(I, -4.0), 4.0));

  if (I > 5.0 || I < -5.0){
    dNsum -= dN * abtastPeriode/1000.0;
  }
}

void countPulse() {
  pulseCount++;
}

void setup() {
  Serial.begin(115200);

  pinMode(pinHighA, OUTPUT);
  pinMode(pinLowA, OUTPUT);
  pinMode(pinHighB, OUTPUT);
  pinMode(pinLowB, OUTPUT);
  pinMode(pinEnable, OUTPUT);

  pinMode(drehzahlPin, INPUT);
  pinMode(drehmomentPin, INPUT);
  pinMode(spannungPin, INPUT);
  pinMode(stromPin, INPUT);

  // Bootstrap vorladen
  digitalWrite(pinEnable, HIGH);
  digitalWrite(pinHighA, LOW);
  digitalWrite(pinLowA, HIGH);
  digitalWrite(pinHighB, LOW);
  digitalWrite(pinLowB, HIGH);
  delay(5);  // 5 ms Bootstrap-Vorladen
  digitalWrite(pinLowA, LOW);
  digitalWrite(pinLowB, LOW);

  attachInterrupt(digitalPinToInterrupt(drehzahlPin), countPulse, RISING);
}

void loop() {

  pulseCount = 0;
  delay(abtastPeriode);

  DrehZahlIstRPM = (pulseCount * 60UL*1000) / (720UL * abtastPeriode);
  if (DrehZahlIstRPM < 100){    // Drehzahlmessung gibt unsinnige Werte zwischen 0 un 100, wenn N=0
    DrehZahlIstRPM = 0;
  }
  
  // Sensor: 5V entspricht 1Nm
  DrehmomentIst = 0;//analogRead(drehmomentPin) * 5.0/1023.0 * 1.0/5.0;

  //Spannungsteiler 
  ZwischenkreisSpannungIst = analogRead(spannungPin) * 5.0/1023.0 * 30.0/5.0; 

  // Strommessung
  float voltage = (analogRead(stromPin) / 1023.0) * 5.0;
  float stromAktuell = (voltage - 2.5) / (3 * 0.1) * 0.9;
  AnkerStromIst = 0.05*AnkerStromIst + 0.05*stromAktuell; // Filter

  // Daten senden: "DrehZahlIstRPM, DrehZahlSollRPM, DrehmomentIst,ZwischenkreisSpannungIst, MotorspannungSoll, PWM, AnkerStromIst"
  Serial.print("<");
  Serial.print(DrehZahlIstRPM);
  Serial.print(",");
  Serial.print(DrehZahlSollRPM);
  Serial.print(",");
  Serial.print(DrehmomentIst, 2);
  Serial.print(",");
  Serial.print(ZwischenkreisSpannungIst, 2);
  Serial.print(",");
  Serial.print(MotorSpannungSollV, 2);
  Serial.print(",");
  Serial.print(PWM, 2);
  Serial.print(",");
  Serial.print(AnkerStromIst, 2);
  Serial.println(">");

  if (MotorMode == 1){
    controlMotorSpeed(); // 1: geregelt, 0: manuell über Spannung
  } 


  while (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    
    if (input.startsWith("U:")) {
      MotorMode = 0;
      double motor_U = input.substring(2).toDouble();
      setMotorVoltage(motor_U);

    }  else if (input.startsWith("N:")) {
      MotorMode = 1;
      int motor_N = input.substring(2).toInt();
      DrehZahlSollRPM = motor_N;

    }  else if (input.startsWith("NKp:")) {
      double Kp = input.substring(4).toDouble();
      if (Kp > 0 && Kp < 10){
        NReglerKp = Kp;
      }

    }  else if (input.startsWith("NKi:")) {
      double Ki = input.substring(4).toDouble();
      if (Ki > 0 && Ki < 10){
        NReglerKi = Ki;
      }

    }  else if (input.startsWith("IKp:")) {
      double Kp = input.substring(4).toDouble();
      if (Kp > 0 && Kp < 10){
        IReglerKp = Kp;
      }

    }  else if (input.startsWith("IKi:")) {
      double Ki = input.substring(4).toDouble();
      if (Ki > 0 && Ki < 10){
        IReglerKi = Ki;
      }

    }
  }
}