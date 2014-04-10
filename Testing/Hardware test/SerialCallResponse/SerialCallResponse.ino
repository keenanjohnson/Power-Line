void setup()
{ 
  
  pinMode(3, OUTPUT);
    // start serial port at 9600 bps:
    
    digitalWrite(3, HIGH); 
    
    delay(1000);
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
