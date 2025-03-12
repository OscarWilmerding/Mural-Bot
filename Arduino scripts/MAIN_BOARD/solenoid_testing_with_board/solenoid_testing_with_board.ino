// Simplified ESP32 GPIO control script

const int pins[] = {18,23,19,25,32,15,33,27,4,16,26,14,13,12};
const int numPins = sizeof(pins) / sizeof(pins[0]);

int durationMs = 100;
int preActivationDelay = 0;
const int fixedPostActivationDelay = 1000;

void setAllPins(bool state) {
  for (int i = 0; i < numPins; i++) {
    digitalWrite(pins[i], state ? HIGH : LOW);
  }
}

void setup() {
  Serial.begin(115200);
  
  for (int i = 0; i < numPins; i++) {
    pinMode(pins[i], OUTPUT);
  }

  randomSeed(analogRead(0));

  Serial.println("GPIO Control Script Initialized.");
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.equalsIgnoreCase("clean")) {
      Serial.println("Starting cleaning cycle...");
      for (int i = 0; i < 120; i++) {
        setAllPins(true);
        delay(500);
        setAllPins(false);
        delay(1000);
      }
      Serial.println("Cleaning cycle complete.");
    }
    else if (input.startsWith("delay")) {
      int spaceIndex = input.indexOf(' ');
      if (spaceIndex != -1) {
        preActivationDelay = input.substring(spaceIndex + 1).toInt();
        Serial.print("Pre-activation delay set to: ");
        Serial.print(preActivationDelay);
        Serial.println(" ms");
      }
    }
    else if (input.equalsIgnoreCase("forever")) {
      Serial.println("Starting forever cleaning cycle...");
      for (;;) { // Infinite loop
        setAllPins(true);
        delay(1000);
        setAllPins(false);
        delay(30000);
      }
    }
    else if (input.equalsIgnoreCase("rand")) {
      Serial.println("Starting random cycle...");
      delay(5000);
      for (int i = 0; i < 10; i++) {
        setAllPins(true);
        delay(durationMs);
        setAllPins(false);
        delay(random(300, 600));
      }
      Serial.println("Random cycle complete.");
    }
    else if (input.equalsIgnoreCase("trig")) {
      Serial.println("Trigger command received.");
      delay(preActivationDelay);
      setAllPins(true);
      delay(durationMs);
      setAllPins(false);
      delay(fixedPostActivationDelay);
    }
    else {
      int newDuration = input.toInt();
      if (newDuration > 0) {
        durationMs = newDuration;
        Serial.print("Activation duration set to: ");
        Serial.print(durationMs);
        Serial.println(" ms");
      }
    }
  }
}
