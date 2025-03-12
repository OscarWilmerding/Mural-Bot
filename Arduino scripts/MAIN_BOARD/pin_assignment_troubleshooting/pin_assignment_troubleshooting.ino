// ESP32 GPIO high for 5 seconds example

const int pins[] = {18,23,19,25,32,15,33,27,4,16,26,14,13,12};
const int numPins = sizeof(pins) / sizeof(pins[0]);

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < numPins; i++) {
    pinMode(pins[i], OUTPUT);
  }
}

void loop() {
  // Set GPIOs high
  Serial.println("GPIOs HIGH");
  for (int i = 0; i < numPins; i++) {
    digitalWrite(pins[i], HIGH);
  }
  delay(5000);

  // Set GPIOs low after 5 seconds
  for (int i = 0; i < numPins; i++) {
    digitalWrite(pins[i], LOW);
  }

  Serial.println("Pins toggled, waiting 5 seconds...");
  delay(5000);
}
