void setup()
{
  // start serial port at 9600 bps:
  Serial.begin(600);
}

void loop()
{
  char inByte;
  
  // if we get a valid byte, read analog ins:
  if (Serial.available()) {
    inByte = Serial.read();
    Serial.println(inByte);   
  }
}
