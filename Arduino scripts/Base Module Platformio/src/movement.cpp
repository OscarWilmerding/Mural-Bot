#include <Arduino.h>
#include <math.h>
#include <float.h>
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

// Helper function to calculate ending pulley lengths for a stripe command
void calculateEndingLengths(const Command& cmd, float& endA, float& endB) {
  // Get all required variables
  float drop = cmd.drop;
  float As = cmd.startPulleyA;
  float Bs = cmd.startPulleyB;
  float Psep = pulleySpacing;
  
  // Calculate end positions
  endA = sqrt(pow(sqrt(-pow(Psep, 0.4e1) + (double) (2 * As * As + 2 * Bs * Bs) * Psep * Psep - (double) ((int) pow((double) (As - Bs), (double) 2) * (int) pow((double) (As + Bs), (double) 2))) / Psep / 0.2e1 + drop, 0.2e1) + pow((double) (As * As) - (double) (Bs * Bs) + Psep * Psep, 0.2e1) * pow(Psep, -0.2e1) / 0.4e1);
  endB = sqrt(pow(sqrt(-pow(Psep, 0.4e1) + (double) (2 * As * As + 2 * Bs * Bs) * Psep * Psep - (double) ((int) pow((double) (As - Bs), (double) 2) * (int) pow((double) (As + Bs), (double) 2))) / Psep / 0.2e1 + drop, 0.2e1) + pow(-(double) (As * As) + (double) (Bs * Bs) + Psep * Psep, 0.2e1) * pow(Psep, -0.2e1) / 0.4e1);
  
  // Serial.println("Input variables:");
  // Serial.print("Drop: "); Serial.println(drop, 6);
  // Serial.print("Start A: "); Serial.println(As, 6);
  // Serial.print("Start B: "); Serial.println(Bs, 6);
  // Serial.print("Pulley separation: "); Serial.println(Psep, 6);
  
  // Serial.println("Calculated end positions:");
  // Serial.print("End A: "); Serial.println(endA, 6);
  // Serial.print("End B: "); Serial.println(endB, 6);
}

// Four corners movement sequence
void four_corners() {
  // Find first and last stripe commands
  Command* firstStripe = nullptr;
  Command* lastStripe = nullptr;
  
  for (int i = 0; i < commandCount; i++) {
    if (commands[i].type == Command::STRIPE) {
      if (firstStripe == nullptr) {
        firstStripe = &commands[i];
      }
      lastStripe = &commands[i];
    }
  }
  
  if (!firstStripe || !lastStripe) {
    Serial.println("Error: No stripe commands found!");
    return;
  }

  // Calculate ending positions for first and last stripes
  float firstEndA, firstEndB, lastEndA, lastEndB;
  calculateEndingLengths(*firstStripe, firstEndA, firstEndB);
  calculateEndingLengths(*lastStripe, lastEndA, lastEndB);
  
  // Corner 1: First stripe starting position
  Serial.println("Moving to Corner 1 (First stripe start)");
  move_to_position_blocking(firstStripe->startPulleyA, firstStripe->startPulleyB);
  Serial.println("Triggering at Corner 1");
  sendSinglePaintBurst();
  delay(5000);  // 5 second wait after trigger
  
  // Corner 2: Last stripe starting position
  Serial.println("Moving to Corner 2 (Last stripe start)");
  move_to_position_blocking(lastStripe->startPulleyA, lastStripe->startPulleyB);
  Serial.println("Triggering at Corner 2");
  sendSinglePaintBurst();
  delay(5000);  // 5 second wait after trigger
  
  // Corner 3: First stripe ending position
  Serial.println("Moving to Corner 3 (First stripe end)");
  move_to_position_blocking(firstEndA, firstEndB);
  Serial.println("Triggering at Corner 3");
  sendSinglePaintBurst();
  delay(5000);  // 5 second wait after trigger
  
  // Corner 4: Last stripe ending position
  Serial.println("Moving to Corner 4 (Last stripe end)");
  move_to_position_blocking(lastEndA, lastEndB);
  Serial.println("Triggering at Corner 4");
  sendSinglePaintBurst();
  delay(5000);  // 5 second wait after trigger
  
  Serial.println("Four corners movement complete");
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
