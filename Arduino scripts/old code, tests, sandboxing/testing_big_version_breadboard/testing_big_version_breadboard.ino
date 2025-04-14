// Pin definitions
const int button1Pin = 9;
const int button2Pin = 2;
const int outputPins[] = {4, 5, 7, 8};

void setup() {
  Serial.begin(115200);

  // Configure buttons as input with pull-up
  pinMode(button1Pin, INPUT_PULLUP);
  pinMode(button2Pin, INPUT_PULLUP);

  // Configure output pins and set LOW initially
  for (int i = 0; i < 4; i++) {
    pinMode(outputPins[i], OUTPUT);
    digitalWrite(outputPins[i], LOW);
  }

  Serial.println("Ready.");
}

void loop() {
  bool button1Pressed = digitalRead(button1Pin) == LOW;
  bool button2Pressed = digitalRead(button2Pin) == LOW;

  if (button1Pressed || button2Pressed) {
    Serial.print("Button ");
    if (button1Pressed) Serial.print("1");
    if (button2Pressed) Serial.print("2");
    Serial.println(" pressed. Setting pins 4, 5, 7, 8 HIGH.");
    
    for (int i = 0; i < 4; i++) {
      digitalWrite(outputPins[i], HIGH);
    }
    delay(200); // simple debounce
  } else {
    for (int i = 0; i < 4; i++) {
      digitalWrite(outputPins[i], LOW);
    }
  }
}
