#include <Arduino.h>


const int drehzahlPin = 2;
const int drehmomentPin = A0;
const int spannungPin = A1; 
const int stromPin = A2;

const int pwm1Pin = 11;           // PWM-Ausgang A-Side (OC1A)
const int pwm2Pin = 12;           // PWM-Ausgang B-Side (OC1B)
const int disablePin = 7;

volatile unsigned long pulseCount = 0;
int abtastrate = 500; // ms

void countPulse() {
  pulseCount++;
}

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

void driveMotor(double U){
  // U zwischen -1 (=-30V) und +1 (30V). Dazwischen PWM

  if (U>1 or U<-1){
    U = max(min(U, 1), -1);
  }
  
  if (U > 0){
    // MOSFETs: LowA = PWM, LowB=0 => HighB=1   
    OCR1A = (int) (U * 800.0);
    OCR1B = 0;

  } else if (U > 0){
    // MOSFETs: LowB = PWM, LowA=0 => HighA=1   
    OCR1A = 0;
    OCR1B = (int) (-U * 800.0);
  } else {
    OCR1A = 0;
    OCR1B = 0;
  }
}


void setup() {
  Serial.begin(9600);

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
  delay(abtastrate);
  unsigned long drehzahl_rpm = (pulseCount * 60UL) / (720 * abtastrate);


  float drehmoment = analogRead(drehmomentPin) * (5.0 / 1023.0);
  float spannung = analogRead(spannungPin) * (5.0 / 1023.0);
  float strom = analogRead(stromPin) * (5.0 / 1023.0);

  // Daten senden: "drehzahl_rpm,drehmoment,spannung,strom"
  Serial.print(drehzahl_rpm);
  Serial.print(",");
  Serial.print(drehmoment, 2);
  Serial.print(",");
  Serial.print(spannung, 2);
  Serial.print(",");
  Serial.println(strom, 2);


  while (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    
    if (input.startsWith("U:")) {
      double motor_U = input.substring(2).toDouble();
      driveMotor(motor_U);

    } else if (input.startsWith("DISABLE:")) {
      int state = input.substring(8).toInt();
      digitalWrite(disablePin, state);
    }
  }
}