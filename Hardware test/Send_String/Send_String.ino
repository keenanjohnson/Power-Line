void setup()
{
  // start serial port at 9600 bps:
  Serial.begin(600);
}

void loop()
{
    Serial.write("A");
    delay(500);
}
