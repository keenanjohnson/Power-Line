/*
  DigitalReadSerial
 Reads a digital input on pin 2, prints the result to the serial monitor 
 
 This example code is in the public domain.
 */

// digital pin 2 has a pushbutton attached to it. Give it a name:
#ifndef TRUE
  #define TRUE 1
  #define FALSE 0
#endif
char testBit = FALSE;

const uint8_t testDataHigh[20] = {'B', 'A', 'L', 'L', 'I', 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
const uint8_t testDataLow[20]  = {'P', 'A', '@', 'W', 'I', 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

int incomingByte = 0;   // for incoming serial data

// the setup routine runs once when you press reset:
void setup() {
  // initialize serial communication at 9600 bits per second:
  Serial.begin(19200);
  
  pinMode(8, OUTPUT);
  pinMode(13, OUTPUT);
  
  delay(1000);
}

void loop() // run over and over
{
  digitalWrite(13, 1);
  Serial.write(testDataHigh,20);
  
  delay(200);

  while( Serial.available() > 0 ) {
    // read the incoming byte:
    incomingByte = Serial.read();
    digitalWrite(13, 0);
  }
  
  delay(300);
}
