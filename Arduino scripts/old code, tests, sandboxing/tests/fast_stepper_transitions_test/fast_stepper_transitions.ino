#include <AccelStepper.h>
//#include <WiFi.h>
//#include <esp_now.h>

// Define motor interface type
#define motorInterfaceType 1  // Using a driver that requires step and direction pins

// Define pins for Motor 1
const int stepPin1 = 5;  // Pulse pin for Motor 1
const int dirPin1 = 4;   // Direction pin for Motor 1

// Define pins for Motor 2
const int stepPin2 = 2;  // Pulse pin for Motor 2
const int dirPin2 = 1;   // Direction pin for Motor 2

// Create instances of the AccelStepper class
AccelStepper stepper1(motorInterfaceType, stepPin1, dirPin1);
AccelStepper stepper2(motorInterfaceType, stepPin2, dirPin2);

// Variables for acceleration and max velocity (steps per second squared and steps per second)
float acceleration = 10000.0;  // Set your desired acceleration here
float maxSpeed = 2000.0;    // Set your desired max speed here

// Number of steps per revolution (considering microstepping)
const int stepsPerRevolution = 200 * 16;  // 200 steps/rev * 16 microsteps

// Number of movements
const int numMovements = 21;

// Arrays to store target rotations for each motor
int rotations1[] = {1, -2, 5, 3, 5, 2, -3,1, -2, 5, 3, 5, 2, -3,1, -2, 5, 3, 5, 2, -3};
int rotations2[] = {-2, 3, -7, -1, -2, -4, 1,-2, 3, -7, -1, -2, -4, 1,-2, 3, -7, -1, -2, -4, 1};

// Movement index
int movementIndex = 0;

// Flags to track movement status
bool movementInProgress = false;
bool motor1Done = false;
bool motor2Done = false;

void setup() {
  // Initialize Serial communication
  Serial.begin(9600);
  Serial.println("Stepper Motor Control Initialized");

  // Set acceleration and max speed for both motors
  stepper1.setAcceleration(acceleration);
  stepper1.setMaxSpeed(maxSpeed);

  stepper2.setAcceleration(acceleration);
  stepper2.setMaxSpeed(maxSpeed);

  // Print initial settings
  Serial.print("Acceleration set to: ");
  Serial.println(acceleration);
  Serial.print("Max speed set to: ");
  Serial.println(maxSpeed);

  // Start the first movement
  startNextMovement();
}

void loop() {
  // If a movement is in progress, run the motors
  if (movementInProgress) {
    bool motor1Running = (stepper1.distanceToGo() != 0);
    bool motor2Running = (stepper2.distanceToGo() != 0);

    if (motor1Running) {
      stepper1.run();  // Move Motor 1 one step towards the target
    } else if (!motor1Done) {
      motor1Done = true;
      Serial.println("Motor 1 Movement Complete");
    }


    if (motor2Running) {
      stepper2.run();  // Move Motor 2 one step towards the target
    } else if (!motor2Done) {
      motor2Done = true;
      Serial.println("Motor 2 Movement Complete");
    }
    

    // Optionally, print the current positions at intervals
    static unsigned long lastPrintTime = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastPrintTime >= 50) {  // Print every 500 milliseconds
      Serial.print("Motor 1 Current Position: ");
      Serial.println(stepper1.currentPosition());
      Serial.print("Motor 2 Current Position: ");
      Serial.println(stepper2.currentPosition());
      lastPrintTime = currentTime;
    }

    // Check if both motors have completed their motions
    if (motor1Done && motor2Done) {
      movementInProgress = false;
      motor1Done = false;
      motor2Done = false;
      Serial.println("Both Motors Movement Complete");

      // Proceed to the next movement
      movementIndex++;
      if (movementIndex < numMovements) {
        startNextMovement();
      } else {
        Serial.println("All Movements Complete");

        // Disable motor outputs
        stepper1.disableOutputs();
        stepper2.disableOutputs();

        while (1);  // Halt the program
      }
    }
  }
}

void startNextMovement() {
  // Calculate the target positions for the next movement
  long targetPosition1 = stepper1.currentPosition() + rotations1[movementIndex] * stepsPerRevolution;
  long targetPosition2 = stepper2.currentPosition() + rotations2[movementIndex] * stepsPerRevolution;

  // Set the target positions
  stepper1.moveTo(targetPosition1);
  stepper2.moveTo(targetPosition2);

  Serial.print("Starting Movement ");
  Serial.println(movementIndex + 1);
  Serial.print("Motor 1 Target Position: ");
  Serial.println(targetPosition1);
  Serial.print("Motor 2 Target Position: ");
  Serial.println(targetPosition2);

  movementInProgress = true;
}
