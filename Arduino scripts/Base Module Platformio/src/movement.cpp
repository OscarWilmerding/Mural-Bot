#include <Arduino.h>
#include <math.h>
#include <AccelStepper.h>
#include "state.h"
#include "modules.h"

// non-blocking movement
void move_to_position(float position1, float position2) {
  long targetPosition1 = position1 * stepsPerMeter * motor1Direction;
  long targetPosition2 = position2 * stepsPerMeter * motor2Direction;

  stepper1.setAcceleration(baseAcceleration * accelerationMultiplier);
  stepper2.setAcceleration(baseAcceleration * accelerationMultiplier);
  stepper1.setMaxSpeed(baseMaxSpeed * maxSpeedMultiplier);
  stepper2.setMaxSpeed(baseMaxSpeed * maxSpeedMultiplier);

  stepper1.moveTo(targetPosition1);
  stepper2.moveTo(targetPosition2);

  Serial.print("Moving to Position (m) - Motor 1: ");
  Serial.print(position1);
  Serial.print(" Motor 2: ");
  Serial.println(position2);

  movementInProgress = true;
}

// blocking movement
void move_to_position_blocking(float position1, float position2) {
  long targetPosition1 = position1 * stepsPerMeter * motor1Direction;
  long targetPosition2 = position2 * stepsPerMeter * motor2Direction;

  stepper1.setAcceleration(baseAcceleration * accelerationMultiplier);
  stepper2.setAcceleration(baseAcceleration * accelerationMultiplier);
  stepper1.setMaxSpeed(baseMaxSpeed * maxSpeedMultiplier);
  stepper2.setMaxSpeed(baseMaxSpeed * maxSpeedMultiplier);

  stepper1.moveTo(targetPosition1);
  stepper2.moveTo(targetPosition2);

  Serial.print("Blocking move to (m):  A=");
  Serial.print(position1);
  Serial.print("  B=");
  Serial.println(position2);

  while ((stepper1.distanceToGo() != 0) || (stepper2.distanceToGo() != 0)) {
    stepper1.run();
    stepper2.run();
  }

  Serial.println("Blocking move complete.");
}

void printCurrentPositions() {
  long steps1 = stepper1.currentPosition();
  long steps2 = stepper2.currentPosition();
  float position1 = steps1 / (stepsPerMeter * motor1Direction);
  float position2 = steps2 / (stepsPerMeter * motor2Direction);

  Serial.print("Positions (m, steps) - 1: ");
  Serial.print(position1);
  Serial.print(", ");
  Serial.print(steps1);
  Serial.print("  Motor 2: ");
  Serial.print(position2);
  Serial.print(", ");
  Serial.println(steps2);
}

void determineStripeVelocities(float posA, float posB, float &velA, float &velB) {
  float Vx = 0;
  float Vy = stripeVelocity * stepsPerMeter;

  float pulleySpacingSteps = pulleySpacing * stepsPerMeter;

  float xPositionSteps = (posA * posA - posB * posB +
                         pulleySpacingSteps * pulleySpacingSteps) /
                         pulleySpacingSteps / 2.0;

  float yPositionSteps = sqrt((4.0 * posA * posA -
                         pow((posA * posA - posB * posB +
                         pulleySpacingSteps * pulleySpacingSteps), 2) *
                         pow(pulleySpacingSteps,  (-2)))) / 2.0;

  float desiredXPositionSteps = stepsPerMeter * Vx * ((float)velocityCalcDelay / 1000)
                              + xPositionSteps;
  float desiredYPositionSteps = stepsPerMeter * stripeVelocity * ((float)velocityCalcDelay / 1000)
                              + yPositionSteps;

  float aLengthDesired = sqrt(desiredXPositionSteps * desiredXPositionSteps
                            + desiredYPositionSteps * desiredYPositionSteps);

  float bLengthDesired = sqrt(pow(desiredXPositionSteps - pulleySpacingSteps, 2)
                            + desiredYPositionSteps * desiredYPositionSteps);

  velA = (aLengthDesired - posA) / ((float)velocityCalcDelay / 1000);
  velB = (bLengthDesired - posB) / ((float)velocityCalcDelay / 1000);
}
