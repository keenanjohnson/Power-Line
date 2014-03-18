#define send_string "Hello"

#define SERIAL_TX_PIN   1
#define SERIAL_RX_PIN   0
#define RX_DATA_IN_PIN  5
#define TX_DATA_OUT_PIN 6

void setup()
{
  // configure in/out modes
  pinMode(RX_DATA_IN_PIN, OUTPUT);
  pinMode(TX_DATA_OUT_PIN, INPUT);
  pinMode(SERIAL_TX_PIN, OUTPUT);
  pinMode(13, OUTPUT);

  // remove both PLNs from dummy state
  digitalWrite(SERIAL_TX_PIN, HIGH);
  digitalWrite(RX_DATA_IN_PIN, HIGH);
  delay(1000); // delay 1 second

  // begin serial transmitting to PLN
  Serial.begin(600);
}

void loop() // run over and over
{
  char inByte;
  const char sendByte = 0xFF;

  if(Serial.available()) {
    inByte = Serial.read();

//    Serial.println("Received byte: ");
//    Serial.println(inByte);

    digitalWrite(13, LOW);
  } else {
    Serial.write(sendByte);
    digitalWrite(13, HIGH);
  }

  delay(1000);
}

