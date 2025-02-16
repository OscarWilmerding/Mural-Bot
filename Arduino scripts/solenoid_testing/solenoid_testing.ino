const int outputPin = 10;
const int buttonPin = 21;
int durationMs = 100;           // Solenoid activation duration (in ms)
int preActivationDelay = 0;     // Delay after button press before solenoid activation (in ms)
const int fixedPostActivationDelay = 1000; // Fixed delay after solenoid activation

void setup() {
  pinMode(outputPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP); // Button between pin 21 and GND
  Serial.begin(9600);
  // Seed the random number generator (using an unconnected analog pin)
  randomSeed(analogRead(0));
  
  // Print initial information
  Serial.println("Default solenoid activation duration set to 100ms.");
  Serial.println("Enter a new duration (in ms) for solenoid activation.");
  Serial.println("Enter 'clean' to initiate a cleaning cycle.");
  Serial.println("Enter 'delay XX' to set a delay (in ms) BEFORE solenoid activation on button press.");
}

void loop() {
  // Process Serial input
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    // Cleaning cycle command
    if (input.equalsIgnoreCase("clean")) {
      Serial.println("Starting cleaning cycle...");
      for (int i = 0; i < 40; i++) {
        digitalWrite(outputPin, HIGH);
        delay(500);       // Pin HIGH for 0.5 sec
        digitalWrite(outputPin, LOW);
        delay(1000);      // Pin LOW for 1 sec
      }
      Serial.println("Cleaning cycle complete.");
    }
    // Delay setting command
    else if (input.startsWith("delay")) {
      int spaceIndex = input.indexOf(' ');
      if (spaceIndex != -1) {
        String delayValueStr = input.substring(spaceIndex + 1);
        delayValueStr.trim();
        int newDelay = delayValueStr.toInt();
        if (newDelay >= 0) {
          preActivationDelay = newDelay;
          Serial.print("Pre-activation delay set to: ");
          Serial.print(preActivationDelay);
          Serial.println(" ms");
        }
      }
    }
    else if (input.equalsIgnoreCase("forever")) {
      Serial.println("Starting cleaning cycle...");
      for (int i = 0; i < 100000; i++) {
        digitalWrite(outputPin, HIGH);
        delay(1000);       // Pin HIGH for 1 sec
        digitalWrite(outputPin, LOW);
        delay(30000);      // Pin LOW for 30 sec
      }
      Serial.println("Cleaning cycle complete.");
    }
    // Random cycle command
    else if (input.equalsIgnoreCase("rand")) {
      Serial.println("Starting random cycle...");
      delay(5000); // Initial 5-second delay
      for (int i = 0; i < 10; i++) {
        // Activate solenoid for the preset duration
        digitalWrite(outputPin, HIGH);
        delay(durationMs);
        digitalWrite(outputPin, LOW);
        
        // Random pause between 500 and 1200 ms
        int randomDelay = random(500, 1201); // upper bound is exclusive
        delay(randomDelay);
      }
      Serial.println("Random cycle complete.");
    }
    // Otherwise, treat as new solenoid activation duration
    else {
      int newDuration = input.toInt();
      if (newDuration > 0) {
        durationMs = newDuration;
        Serial.print("Solenoid activation duration set to: ");
        Serial.print(durationMs);
        Serial.println(" ms");
      }
    }
  }

  // Button press handling (active LOW)
  if (digitalRead(buttonPin) == LOW) {
    Serial.println("Button pressed, initiating sequence...");

    // Pre-activation delay from Serial input
    delay(preActivationDelay);

    // Activate solenoid for durationMs
    digitalWrite(outputPin, HIGH);
    delay(durationMs);
    digitalWrite(outputPin, LOW);

    // Fixed delay after solenoid activation
    delay(fixedPostActivationDelay);
  }
}
