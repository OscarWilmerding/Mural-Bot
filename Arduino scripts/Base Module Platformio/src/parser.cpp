#include <Arduino.h>
#include "state.h"
#include "modules.h"

void listAvailableCommands() {
  Serial.println("Available Commands:");
  Serial.println("  go                          - Start the next command in sequence");
  Serial.println("  run                         - Run all commands from current position");
  Serial.println("  move a to X                 - Move Motor 1 to X meters (absolute)");
  Serial.println("  move b to X                 - Move Motor 2 to X meters (absolute)");
  Serial.println("  move a X                    - Move Motor 1 by X meters (relative)");
  Serial.println("  move b X                    - Move Motor 2 by X meters (relative)");
  Serial.println("  zero a                      - Set Motor 1 position to zero");
  Serial.println("  zero b                      - Set Motor 2 position to zero");
  Serial.println("  set a to X                  - Set Motor 1 position to X meters");
  Serial.println("  set b to X                  - Set Motor 2 position to X meters");
  Serial.println("  acceleration multiplier X   - Set acceleration multiplier to X");
  Serial.println("  velocity multiplier X       - Set velocity multiplier to X");
  Serial.println("  set stripe velocity X       - Set the global stripeVelocity (m/s)");
  Serial.println("  set velocity calc delay X   - Set ms delay between velocity recalculations");
  Serial.println("  set confirmation timeout X  - Set confirmation timeout (ms)");
  Serial.println("  restart                     - Restart the ESP32");
  Serial.println("  reset run                   - Reset run index to 0");
  Serial.println("  pre poke pause X            - Set pause (ms) before sending trigger");
  Serial.println("  chasseyWaitTime X           - Set chassis wait time (ms)");
  Serial.println("  skip color                  - Skip commands until next COLOR_CHANGE");
  Serial.println("  test                        - Trigger chassis without movement");
  Serial.println("  set command index X         - Set current command index");
  Serial.println("  spr XX                      - Set steps per meter to XX");
  Serial.println("  4corners                    - Move to four corners of the mural");
  Serial.println("  ?                           - Show this help list");
}

void processSerialCommand(String command) {
  command.trim();

  if (command == "?") {
    listAvailableCommands();
  }
  else if (command.startsWith("move a to ")) {
    float position = command.substring(10).toFloat();
    motor1Position = position;
    stepper1.moveTo(position * stepsPerMeter * motor1Direction);
    Serial.print("Motor 1 moving to position (m): ");
    Serial.println(position);
    movementInProgress = true;
  }
  else if (command.startsWith("move b to ")) {
    float position = command.substring(10).toFloat();
    motor2Position = position;
    stepper2.moveTo(position * stepsPerMeter * motor2Direction);
    Serial.print("Motor 2 moving to position (m): ");
    Serial.println(position);
    movementInProgress = true;
  }
  else if (command.startsWith("move a ")) {
    float distance = command.substring(7).toFloat();
    stepper1.move(distance * stepsPerMeter * motor1Direction);
    Serial.print("Motor 1 moving by (m): ");
    Serial.println(distance);
    movementInProgress = true;
  }
  else if (command.startsWith("move b ")) {
    float distance = command.substring(7).toFloat();
    stepper2.move(distance * stepsPerMeter * motor2Direction);
    Serial.print("Motor 2 moving by (m): ");
    Serial.println(distance);
    movementInProgress = true;
  }
  else if (command.startsWith("set a to ")) {
    float position = command.substring(9).toFloat();
    motor1Position = position;
    stepper1.setCurrentPosition(position * stepsPerMeter * motor1Direction);
    Serial.print("Motor 1 position set to (m): ");
    Serial.println(position);
  }
  else if (command.startsWith("set b to ")) {
    float position = command.substring(9).toFloat();
    motor2Position = position;
    stepper2.setCurrentPosition(position * stepsPerMeter * motor2Direction);
    Serial.print("Motor 2 position set to (m): ");
    Serial.println(position);
  }
  else if (command.startsWith("acceleration multiplier ")) {
    float multiplier = command.substring(23).toFloat();
    float prev = baseAcceleration * accelerationMultiplier;
    accelerationMultiplier = multiplier;
    float newA = baseAcceleration * accelerationMultiplier;
    stepper1.setAcceleration(newA);
    stepper2.setAcceleration(newA);
    Serial.print("Acceleration multiplier set to: ");
    Serial.println(multiplier);
    Serial.print("Previous acceleration: ");
    Serial.println(prev);
    Serial.print("New acceleration: ");
    Serial.println(newA);
  }
  else if (command.startsWith("velocity multiplier ")) {
    float multiplier = command.substring(19).toFloat();
    float prev = baseMaxSpeed * maxSpeedMultiplier;
    maxSpeedMultiplier = multiplier;
    float newS = baseMaxSpeed * maxSpeedMultiplier;
    stepper1.setMaxSpeed(newS);
    stepper2.setMaxSpeed(newS);
    Serial.print("Velocity multiplier set to: ");
    Serial.println(multiplier);
    Serial.print("Previous max speed: ");
    Serial.println(prev);
    Serial.print("New max speed: ");
    Serial.println(newS);
  }
  else if (command.startsWith("spr ")) {
    int newSPR = command.substring(4).toInt();
    if (newSPR > 0) {
      stepsPerMeter = newSPR;
      Serial.print("Steps-per-meter set to: ");
      Serial.println(stepsPerMeter);
    } else {
      Serial.println("Invalid SPR value (must be > 0)");
    }
  }
  else if (command == "zero a") {
    motor1Position = 0.0;
    stepper1.setCurrentPosition(0);
    Serial.println("Motor 1 zeroed");
  }
  else if (command == "zero b") {
    motor2Position = 0.0;
    stepper2.setCurrentPosition(0);
    Serial.println("Motor 2 zeroed");
  }
  else if (command == "go") {
    startNextCommand();
    sendTriggerAfterMovement = true;
  }
  else if (command == "run") {
    runMode = true;
    Serial.println("Starting continuous run mode. Send any character to stop.");
    while (runMode && currentCommandIndex < commandCount) {
      if (Serial.available()) {
        runMode = false;
        Serial.println("\nRun mode interrupted by user input");
        Serial.read(); // Clear the input
        break;
      }
      startNextCommand();
      if (commands[currentCommandIndex-1].type == Command::STRIPE) {
        // After each stripe, briefly check for serial input
        delay(100);  // Small delay to allow for serial input
        if (Serial.available()) {
          runMode = false;
          Serial.println("\nRun mode interrupted by user input");
          Serial.read(); // Clear the input
          break;
        }
      }
    }
  }
  else if (command.startsWith("set confirmation timeout ")) {
    unsigned long newTimeout = command.substring(24).toInt();
    confirmationTimeout = newTimeout;
    Serial.print("Confirmation timeout set to: ");
    Serial.print(confirmationTimeout);
    Serial.println(" ms");
  }
  else if (command == "restart") {
    Serial.println("Restarting ESP32...");
    ESP.restart();
  }
  else if (command == "reset run") {
    currentCommandIndex = 0;
    Serial.println("Run index reset. Ready to run from the beginning.");
  }
  else if (command.startsWith("pre poke pause ")) {
    unsigned long pauseDuration = command.substring(15).toInt();
    prePokePause = pauseDuration;
    Serial.print("Pre-poke pause set to: ");
    Serial.print(prePokePause);
    Serial.println(" ms");
  }
  else if (command.startsWith("chasseyWaitTime ")) {
    unsigned long waitTime = command.substring(15).toInt();
    chassisWaitTime = waitTime;
    Serial.print("Chassis wait time set to: ");
    Serial.print(chassisWaitTime);
    Serial.println(" ms");
  }
  else if (command == "skip color") {
    int originalIndex = currentCommandIndex;
    while (currentCommandIndex < commandCount && commands[currentCommandIndex].type != Command::COLOR_CHANGE) {
      currentCommandIndex++;
    }
    if (currentCommandIndex >= commandCount) {
      Serial.println("No further COLOR_CHANGE commands found. All remaining moves skipped.");
    } else {
      Serial.print("Skipped commands from index ");
      Serial.print(originalIndex + 1);
      Serial.print(" to next COLOR_CHANGE at index ");
      Serial.println(currentCommandIndex + 1);
    }
    printCurrentPositions();
  }
  else if (command == "test") {
    Serial.println("sending big data");
    String bigData = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";
    startLargeStringSend(bigData);
  }
  else if (command.startsWith("set command index ")) {
    int newIndex = command.substring(18).toInt();
    if (newIndex >= 0 && newIndex < commandCount) {
      currentCommandIndex = newIndex;
      Serial.print("Command index set to: ");
      Serial.println(currentCommandIndex);
    } else {
      Serial.println("Invalid index. Out of range.");
    }
    printCurrentPositions();
  }
  else if (command.startsWith("set stripe velocity ")) {
    float newVel = command.substring(20).toFloat();
    stripeVelocity = newVel;
    Serial.print("Stripe velocity set to: ");
    Serial.println(stripeVelocity);
  }
  else if (command.startsWith("set velocity calc delay ")) {
    unsigned long newDelay = command.substring(24).toInt();
    velocityCalcDelay = newDelay;
    Serial.print("Velocity recalculation delay set to: ");
    Serial.print(velocityCalcDelay);
    Serial.println(" ms");
  }
  else if (command == "4 corners" || command == "4corners") {
    four_corners();
  }
  else {
    Serial.println("Invalid command. Type '?' for a list of commands.");
  }

  if (!movementInProgress) {
    printCurrentPositions();
  }
}

void startNextCommand() {
  if (currentCommandIndex < commandCount) {
    Command &cmd = commands[currentCommandIndex];

    if (cmd.type == Command::MOVE) {
      move_to_position(cmd.pos.x, cmd.pos.y);
      Serial.print("Starting MOVE command #");
      Serial.println(currentCommandIndex + 1);
      currentCommandIndex++;
    }
    else if (cmd.type == Command::COLOR_CHANGE) {
      Serial.print("COLOR CHANGE TO ");
      Serial.println(cmd.colorHex);
      runMode = false;
      currentCommandIndex++;
    }
    else if (cmd.type == Command::STRIPE) {
      Serial.print("Executing STRIPE command #");
      Serial.println(currentCommandIndex + 1);

      String stripeData = "{";
      stripeData += "\"stripeName\":\"" + cmd.stripeName + "\",";
      stripeData += "\"drop\":" + String(cmd.drop, 4) + ",";
      stripeData += "\"startPulleyA\":" + String(cmd.startPulleyA, 4) + ",";
      stripeData += "\"startPulleyB\":" + String(cmd.startPulleyB, 4) + ",";
      stripeData += "\"pattern\":" + cmd.pattern + ",";
      stripeData += "\"stripeVelocity\":" + String(stripeVelocity, 4);
      stripeData += "}";

      Serial.println("Generated JSON Data for STRIPE:");
      Serial.println(stripeData);

      Serial.println("Moving to initial stripe position...");
      move_to_position_blocking(cmd.startPulleyA, cmd.startPulleyB);

      stepper1.setAcceleration(0);
      stepper2.setAcceleration(0);
      stepper1.setMaxSpeed(8000);
      stepper2.setMaxSpeed(8000);

      Serial.println("Done moving to initial position.");
      delay(2000);

      startLargeStringSend(stripeData);

      float timeForMovementSeconds = cmd.drop / stripeVelocity;
      float timeForMovementMs = timeForMovementSeconds * 1000.0;

      Serial.print("Time for movement (seconds): ");
      Serial.println(timeForMovementSeconds);
      Serial.print("Time for movement (ms): ");
      Serial.println(timeForMovementMs);

      unsigned long startTime = millis();
      unsigned long lastCallTime = millis();

      Serial.println("Entering stripe movement loop...");
      printCurrentPositions();
      while (millis() - startTime < timeForMovementMs) {
        unsigned long currentTime = millis();
        stepper1.runSpeed();
        stepper2.runSpeed();

        if (currentTime - lastCallTime >= velocityCalcDelay) {
          float posA = stepper1.currentPosition() * motor1Direction;
          float posB = stepper2.currentPosition() * motor2Direction;

          if (posA < 0 || posB < 0) {
            Serial.println("CRITICAL - one value going into velocity func is negative");
          }

          float velocityA, velocityB;
          determineStripeVelocities(posA, posB, velocityA, velocityB);

          stepper1.setSpeed(-1 * velocityA);
          stepper2.setSpeed(-1 * velocityB);

          stepper1.runSpeed();
          stepper2.runSpeed();

          lastCallTime = currentTime;
        }
      }

      Serial.println("Finished stripe movement loop.");
      printCurrentPositions();

      movementInProgress = false;
      currentCommandIndex++;

      stepper1.setCurrentPosition(stepper1.currentPosition());
      stepper2.setCurrentPosition(stepper2.currentPosition());
      
      // Don't delay if we're not in run mode
      if (!runMode) return;
    }

  } else {
    Serial.println("All Commands Complete");
    stepper1.disableOutputs();
    stepper2.disableOutputs();
    runMode = false;
  }
}
