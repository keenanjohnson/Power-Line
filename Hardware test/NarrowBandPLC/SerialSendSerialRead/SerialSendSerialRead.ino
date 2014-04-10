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
const uint8_t testDataLow[20]  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

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
  if( testBit ) {
    Serial.write(testDataHigh,20);
    testBit = FALSE;
  } else {
    Serial.write(testDataLow,20);
    testBit = TRUE;
  }
  
  delay(500);
  
  while( Serial.available() > 0 ) {
    // read the incoming byte:
    incomingByte = Serial.read();
    digitalWrite(13, 1);
    
    delay(500);
  }

  if( !Serial.available() ) {
    digitalWrite(13, 0);
  }
  
  delay(500);
}



