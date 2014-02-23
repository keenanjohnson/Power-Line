void setup()
{
  // start serial port at 9600 bps:
  pinMode(3, OUTPUT);
  
  digitalWrite(3, HIGH); 
  delay(1000);
  Serial.begin(600);
 
}

void loop()
{
   // Serial.write("A");
    delay(500);
}
