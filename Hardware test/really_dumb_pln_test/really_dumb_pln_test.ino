void setup()
{
  // configure in/out modes
  pinMode(4, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(2, INPUT);

  // remove both PLNs from dummy state
  digitalWrite(4, HIGH);
  digitalWrite(3, HIGH);
  delay(10000); // delay 1 second
  
  Serial.begin(9600);
  
  digitalWrite(4, LOW);
}

void loop() // run over and over
{
  if(digitalRead(4) == LOW)
  {
     digitalWrite(4, HIGH);
  }
  else
  {
     digitalWrite(4, LOW);  
  }
  delay(20);
  int State = digitalRead(2);
  Serial.println(State);
}


