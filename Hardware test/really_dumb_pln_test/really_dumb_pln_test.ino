void setup()
{
  // configure in/out modes
  pinMode(4, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(2, INPUT);

  // remove both PLNs from dummy state
  digitalWrite(4, HIGH);
  digitalWrite(3, HIGH);
  delay(1000); // delay 1 second
  
  Serial.begin(9600);
}

void loop() // run over and over
{
  digitalWrite(4, LOW);
  int State = digitalRead(2);
  Serial.println(State);
  delay(500);
}

