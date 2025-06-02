#include <Arduino.h>


const int drehzahlPin = 2;
const int drehmomentPin = A0;
const int spannungPin = A1; 
const int stromPin = A2;

const int pwm1Pin = 11;           // PWM-Ausgang A-Side (OC1A)
const int pwm2Pin = 12;           // PWM-Ausgang B-Side (OC1B)
const int disablePin = 7;

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

float ReglerKp = 0;
float ReglerKi = 0;

float dN = 0;
float dNsum = 0;

float u;


void setup_PWM(){
  pinMode(pwm1Pin, OUTPUT);
  pinMode(pwm2Pin, OUTPUT);

  // PWM-Frequenz: 20kHz
  ICR1 = 799;  // 16 MHz / (799 + 1) = 20 kHz

  // Duty Cycle 
  OCR1A = 0;
  OCR1B = 0;

  TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(WGM11);
  TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);
} 

void setMotorVoltage(float sollSpannung){
  // U zwischen -30V und +30V. Dazwischen PWM

  // PWM aus Sollmotorspannung und istZwischenkreisfrequenz mit PWM begrenzung auf 90% wegen dem Bootstrap vorm Gatetreiber
  float MaxPWM = 0.9;
  MotorSpannungSollV = sollSpannung;
  PWM = abs(MotorSpannungSollV)/ZwischenkreisSpannungIst;

  if (PWM > MaxPWM || PWM < -MaxPWM){
    PWM = max(min(PWM, MaxPWM), -MaxPWM);
  }
  
  if (PWM > 0){
    // MOSFETs: LowA = PWM, LowB=0 => HighB=1   
    OCR1A = (int) (PWM * 800.0);
    OCR1B = 0;

  } else if (PWM < 0){
    // MOSFETs: LowB = PWM, LowA=0 => HighA=1   
    OCR1A = 0;
    OCR1B = (int) (-PWM * 800.0);
  } else {
    OCR1A = 0;
    OCR1B = 0;
  }
}

void controlMotorSpeed(){
  // TODO: Regelung mit ist-Strom implememtieren
  // U = (Nsoll-Nist)(Kp+Ki/s)+cw*Nist aber cw unbekannt, regelt er das vllt selber aus?
  double dN = DrehZahlSollRPM-DrehZahlIstRPM;
  dNsum += dN * abtastPeriode/1000.0;

  double U = dN*ReglerKp + dNsum*ReglerKi;
  setMotorVoltage(min(max(U, 30.0), -30.0));

  if (U > 30.0 || U < -30.0){
    dNsum -= dN * abtastPeriode/1000.0;
  }
  
}

void countPulse() {
  pulseCount++;
}

void setup() {
  Serial.begin(115200);

  pinMode(drehzahlPin, INPUT);
  pinMode(drehmomentPin, INPUT);
  pinMode(spannungPin, INPUT);
  pinMode(stromPin, INPUT);

  attachInterrupt(digitalPinToInterrupt(drehzahlPin), countPulse, RISING);
  pinMode(disablePin, OUTPUT);
  digitalWrite(disablePin, true); //disable den GateTreiber beim initialisieren

  setup_PWM();
}

void loop() {

  pulseCount = 0;
  delay(abtastPeriode);

  DrehZahlIstRPM = (pulseCount * 60UL*1000) / (720UL * abtastPeriode);

  // Sensor: 5V entspricht 1Nm
  DrehmomentIst = analogRead(drehmomentPin) * 5.0/1023.0 * 1.0/5.0;

  //Spannungsteiler mit 27k und 5k: 30V werden zu 4,6V
  ZwischenkreisSpannungIst = analogRead(spannungPin) * 5.0/1023.0 * 32.0/5.0; 

  float shunt_widerstand = 0.020;
  float verstaerkung = 20.0;
  float strom = analogRead(stromPin) * (5.0 / 1023.0);
  AnkerStromIst = (strom-2.5)/verstaerkung/shunt_widerstand;

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
    controlMotorSpeed();
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

    }  else if (input.startsWith("Kp:")) {
      double Kp = input.substring(3).toDouble();
      if (Kp > 0 && Kp < 10){
        ReglerKp = Kp;
      }

    }  else if (input.startsWith("Ki:")) {
      double Ki = input.substring(3).toDouble();
      if (Ki > 0 && Ki < 10){
        ReglerKi = Ki;
      }

    } else if (input.startsWith("DISABLE:")) {
      int state = input.substring(8).toInt();
      digitalWrite(disablePin, state);
    }
  }
}