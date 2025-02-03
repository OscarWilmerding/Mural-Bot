#include <AccelStepper.h>

#define motorInterfaceType 1

// Pins
const int stepPin1 = 5;
const int dirPin1  = 4;
const int stepPin2 = 7;
const int dirPin2  = 8;

// Create steppers
AccelStepper stepper1(motorInterfaceType, stepPin1, dirPin1);
AccelStepper stepper2(motorInterfaceType, stepPin2, dirPin2);

void setup() {
  Serial.begin(115200);
  // Make parseInt() return sooner if there's no incoming data
  Serial.setTimeout(50);

  // High max speed so we don't clamp our test speed
  stepper1.setMaxSpeed(8000);
  stepper2.setMaxSpeed(8000);

  // Run with zero acceleration
  stepper1.setAcceleration(0);
  stepper2.setAcceleration(0);

  Serial.println("Enter a speed in steps/s (e.g., '2000'). Motor runs for 2 seconds.");
}

void loop() {
  static bool running = false;
  static unsigned long startTime = 0;
  static float testSpeed = 0.0;

  // Check if user typed something
  if (Serial.available() > 0) {
    int s = Serial.parseInt(); 
    if (s != 0) {
      testSpeed = (float)s;
      // Immediately configure the steppers
      stepper1.setSpeed(testSpeed);
      stepper2.setSpeed(testSpeed);

      running = true;
      startTime = millis();

      Serial.print("Running at ");
      Serial.print(testSpeed);
      Serial.println(" steps/s for 2 seconds...");
    }
  }

  // If we should be running the motor
  if (running) {
    unsigned long elapsed = millis() - startTime;

    // Continuously call runSpeed() during the time window
    if (elapsed < 2000) {
      stepper1.runSpeed();
      stepper2.runSpeed();
    } else {
      // Time's up, stop motors
      stepper1.setSpeed(0);
      stepper2.setSpeed(0);
      running = false;
      Serial.println("Done. Enter another speed for a new test.\n");
    }
  }
}
