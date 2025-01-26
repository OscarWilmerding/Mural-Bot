
/*
  NOTE: 
  1) We have re-introduced the struct-based approach for storing commands.
  2) There are two placeholder functions:
       - determinePulleyPositionsStripe(...)
       - determineStripeVelocities(...)
     You will fill in the internal logic for those.
  3) We have added a new global variable "velocityCalcDelay" and a serial command
     "set velocity calc delay X" for adjusting how often you might recalculate velocities.
  4) Stripes are still run at indefinite speed (they do not automatically stop).
*/

#include <AccelStepper.h>
#include "LittleFS.h"
#include <esp_now.h>
#include <WiFi.h>

// --------------------- Pin and Motor Setup ----------------------
#define motorInterfaceType 1  // Using a driver that requires step and direction pins

// Define pins for Motor 1
const int stepPin1 = 7;
const int dirPin1  = 8; 

// Define pins for Motor 2
const int stepPin2 = 5;
const int dirPin2  = 4;  

// Create instances of the AccelStepper class
AccelStepper stepper1(motorInterfaceType, stepPin1, dirPin1);
AccelStepper stepper2(motorInterfaceType, stepPin2, dirPin2);

// --------------------- Global Variables -------------------------
bool  sendTriggerAfterMovement  = false;
bool  runMode                   = false;
bool  movementInProgress        = false;
bool  motor1Done                = false;
bool  motor2Done                = false;

// For acceleration and max velocity
float baseAcceleration          = 2000.0 * 2.0;
float baseMaxSpeed              = 400.0  * 2.0;
float accelerationMultiplier    = 1.0;
float maxSpeedMultiplier        = 1.0;

// ESP-NOW variables
uint8_t chassisAddress[]        = { 0x48, 0x27, 0xE2, 0xE6, 0xE8, 0x34 };
bool    commandConfirmed        = false;
const   uint8_t COMMAND_RUN     = 0x01;
typedef struct struct_message {
  uint8_t command;
} struct_message;
struct_message outgoingMessage;

bool          waitingForConfirmation = false;
unsigned long confirmationStartTime   = 0;
unsigned long confirmationTimeout     = 5000;  // Default 5s timeout waiting for response

// Conversion factor (meters -> steps)
const float stepsPerMeter = 10494.55 * 2.0;

// Direction control variables
int motor1Direction = -1;
int motor2Direction = -1;

// Store motor positions in meters
float motor1Position = 0.0;
float motor2Position = 0.0;

// Pre/post movement timing
unsigned long prePokePause   = 0;
unsigned long chassisWaitTime = 2500;

// Additional data from G-code
float pulleySpacing   = 0.0;  
int   numberOfDrawnColumns  = 0;    
float stripeVelocity        = 1.0;  // default to 1 m/s for stripes

// New global for controlling how often (ms) you recalc velocities
unsigned long velocityCalcDelay = 500; // default 500 ms

// ------------------ Command and Stripe Structures --------------
struct Position {
  float x;
  float y;
};

struct Command {
  enum Type {
    MOVE,
    COLOR_CHANGE,
    STRIPE
  } type;

  // For MOVE
  Position pos;     

  // For COLOR_CHANGE
  String colorHex;  

  // For STRIPE
  String stripeName;      
  float drop;             
  float startPulleyA;     
  float startPulleyB;     
  String pattern;         
};

// Array for storing commands
const int MAX_COMMANDS = 1500;
Command commands[MAX_COMMANDS];
int     commandCount        = 0;
int     currentCommandIndex = 0;

/***************************************************************/
/*        Placeholder Functions for Stripe Motion Logic        */
/***************************************************************/

// 2) Determine velocities based on those positions (fill out with your equation)
void determineStripeVelocities(float posA, float posB, float &velA, float &velB) {
  // TODO: Replace with your equation that ensures a certain chassis velocity, etc.
  // For demonstration, set both to half the global stripeVelocity
  float Vx = 0; //no movement in x direction.
  float Vy = 1; //this means the velocity in y direction is positive, which is moving down the wall.

  //these equations were extracted from 'new velocity math' 
  velA = pow(-pow(pulleySpacing,4.0f)+(2.0f*posA*posA+2.0f*posB*posB)*pulleySpacing*pulleySpacing-pow((posA-posB),2.0f)*pow((posA+posB),2.0f),-0.5f)*posA*(Vx*sqrt(-pow(pulleySpacing,4.0f)+(2.0f*posA*posA+2.0f*posB*posB)*pulleySpacing*pulleySpacing-pow((posA-posB),2.0f)*pow((posA+posB),2.0f))-Vy*(posA*posA-posB*posB-pulleySpacing*pulleySpacing))/pulleySpacing; 
  velB = -posB*(Vx*sqrt(-pow(pulleySpacing,4.0f)+(2.0f*posA*posA+2.0f*posB*posB)*pulleySpacing*pulleySpacing-pow((posA-posB),2.0f)*pow((posA+posB),2.0f))-Vy*(posA*posA-posB*posB+pulleySpacing*pulleySpacing))*pow(-pow(pulleySpacing,4.0f)+(2.0f*posA*posA+2.0f*posB*posB)*pulleySpacing*pulleySpacing-pow((posA-posB),2.0f)*pow((posA+posB),2.0f),-0.5f)/pulleySpacing;
}

// ------------------- Forward Declarations -----------------------
void onDataSent(const uint8_t *macAddr, esp_now_send_status_t status);
void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len);
void move_to_position(float position1, float position2);
void processSerialCommand(String command);
void startNextCommand();
void listAvailableCommands();
void loadCommandsFromFile(const char *path);
void sendTriggerCommand();
void handleSendTriggerCommand();
void printCurrentPositions();

// ------------------------ Setup Function -------------------------
void setup() {
  Serial.begin(115200);
  Serial.println("Stepper Motor Control Initialized");

  // Set acceleration and max speed
  stepper1.setAcceleration(baseAcceleration * accelerationMultiplier);
  stepper1.setMaxSpeed(baseMaxSpeed * maxSpeedMultiplier);
  stepper2.setAcceleration(baseAcceleration * accelerationMultiplier);
  stepper2.setMaxSpeed(baseMaxSpeed * maxSpeedMultiplier);

  // Initialize LittleFS and load the file
  if (!LittleFS.begin()) {
    Serial.println("An error has occurred while mounting LittleFS");
    return;
  }
  loadCommandsFromFile("/gcode.txt");

  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  Serial.println("ESP-NOW Hub Initialized");

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register callbacks
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  // Add peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, chassisAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

// ------------------------ Main Loop -----------------------------
void loop() {
  // Check Serial for user commands
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    if (runMode) {
      runMode = false;
      Serial.println("Run Mode Interrupted by User Input");
    }
    processSerialCommand(command);
    Serial.println();
  }

  // Always call stepper run methods
  stepper1.run();
  stepper2.run();

  // Check if movement is complete
  if (movementInProgress) {
    bool motor1Running = (stepper1.distanceToGo() != 0);
    bool motor2Running = (stepper2.distanceToGo() != 0);

    if (!motor1Running && !motor1Done) {
      motor1Done = true;
      Serial.println("Motor 1 Movement Complete");
    }
    if (!motor2Running && !motor2Done) {
      motor2Done = true;
      Serial.println("Motor 2 Movement Complete");
    }

    if (motor1Done && motor2Done) {
      movementInProgress = false;
      motor1Done         = false;
      motor2Done         = false;
      Serial.println("Both Motors Movement Complete");

      printCurrentPositions();

      if (prePokePause > 0) {
        Serial.print("Waiting for pre-poke pause: ");
        Serial.print(prePokePause);
        Serial.println(" ms");
        delay(prePokePause);
        Serial.println("Pre-poke pause completed.");
      }

      if (runMode || sendTriggerAfterMovement) {
        sendTriggerCommand();
        sendTriggerAfterMovement = false;
      }
      Serial.println();
    }
  }

  // Handle sendTriggerCommand timeouts/confirmation
  handleSendTriggerCommand();
}

// ------------------ Callback: Data Sent --------------------------
void onDataSent(const uint8_t *macAddr, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
  delay(50);
}

// ------------------ Callback: Data Received -----------------------
void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  Serial.println("Confirmation received from Chassis");
  commandConfirmed = true;
}

// ------------------ Movement Helper Function ----------------------
void move_to_position(float position1, float position2) {
  long targetPosition1 = position1 * stepsPerMeter * motor1Direction;
  long targetPosition2 = position2 * stepsPerMeter * motor2Direction;

  stepper1.moveTo(targetPosition1);
  stepper2.moveTo(targetPosition2);

  Serial.print("Moving to Position (m) - Motor 1: ");
  Serial.print(position1);
  Serial.print(" Motor 2: ");
  Serial.println(position2);

  movementInProgress = true;
}

// ------------------ Process Serial Commands -----------------------
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
    startNextCommand();
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
    Serial.println("Triggering chassis without movement...");
    sendTriggerCommand();
    printCurrentPositions();
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
  // ------ NEW: set velocity calc delay ------
  else if (command.startsWith("set velocity calc delay ")) {
    unsigned long newDelay = command.substring(24).toInt();
    velocityCalcDelay = newDelay;
    Serial.print("Velocity recalculation delay set to: ");
    Serial.print(velocityCalcDelay);
    Serial.println(" ms");
  }
  else {
    Serial.println("Invalid command. Type '?' for a list of commands.");
  }

  // If not starting a movement, print current positions
  if (!movementInProgress) {
    printCurrentPositions();
  }
}

// ------------------ startNextCommand -----------------------------
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
/******************************************************/
/*   Snippet for the STRIPE portion in startNextCommand()   */
/******************************************************/

    else if (cmd.type == Command::STRIPE) {
      // Print which stripe command we're on
      Serial.print("Executing STRIPE command #");
      Serial.println(currentCommandIndex + 1);

      // Demonstrate referencing all relevant fields for this stripe
      Serial.print("  Stripe Name: ");
      Serial.println(cmd.stripeName);

      Serial.print("  Drop Value: ");
      Serial.println(cmd.drop);

      Serial.print("  Starting Position of Pulley A for stripe: ");
      Serial.println(cmd.startPulleyA);

      Serial.print("  Starting Position of Pulley B for stripe: ");
      Serial.println(cmd.startPulleyB);

      Serial.print("  Pattern String: ");
      Serial.println(cmd.pattern);

      move_to_position(cmd.startPulleyA, cmd.startPulleyB); //moves the chassis to the starting position of the stripe and has it wait there for a sec
      delay(1000);

      // Zero out acceleration for indefinite-speed stripe motion, I dont think this is necissary for what I have planned
      // stepper1.setAcceleration(0);
      // stepper2.setAcceleration(0);

      float timeForMovement = cmd.drop / stripeVelocity;

      unsigned long startTime = millis();
      unsigned long lastCallTime = millis();

      while (millis() - startTime < timeForMovement) {
        unsigned long currentTime = millis();

        // Check if it's time to call velocity alter function
        if (currentTime - lastCallTime >= velocityCalcDelay) {  //this is the code that asynchrounously is setting the velocities of the motors during movement

          float posA = stepper1.currentPosition() / (stepsPerMeter * motor1Direction); //get positions, for the first movement this is redundant but for the rest the velocities required are dependent on position
          float posB = stepper2.currentPosition() / (stepsPerMeter * motor2Direction);

          float velocityA, velocityB;
          determineStripeVelocities(posA, posB, velocityA, velocityB);

          float speedA = velocityA * stepsPerMeter * motor1Direction;
          float speedB = velocityB * stepsPerMeter * motor2Direction;

          stepper1.setSpeed(speedA); //set the steppers to the velocity needed
          stepper2.setSpeed(speedB);

          lastCallTime = currentTime; // Update the last call time, necissary for anync loop
        }
      }

      // 1) Determine the current pully positions ()
      float posA = stepper1.currentPosition() / (stepsPerMeter * motor1Direction);
      float posB = stepper2.currentPosition() / (stepsPerMeter * motor2Direction);

      // 2) Compute velocities from those positions (placeholder func)
      float velocityA, velocityB;
      determineStripeVelocities(posA, posB, velocityA, velocityB);

      // Convert m/s -> steps/s
      float speedA = velocityA * stepsPerMeter * motor1Direction;
      float speedB = velocityB * stepsPerMeter * motor2Direction;

      // Indefinite movement warning
      Serial.println("WARNING: This STRIPE runs indefinitely (no automatic stop).");

      // Set speed but do not call moveTo(), so it spins continuously
      stepper1.setSpeed(speedA);
      stepper2.setSpeed(speedB);

      // Not using movementInProgress for indefinite spins
      movementInProgress = false;

      // Move on to the next command in the sequence
      currentCommandIndex++;
    }

  }
  else {
    Serial.println("All Commands Complete");
    stepper1.disableOutputs();
    stepper2.disableOutputs();
    runMode = false;
  }
}

// ------------------ listAvailableCommands -------------------------
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
  Serial.println("  ?                           - Show this help list");
}

// ------------------ loadCommandsFromFile --------------------------
void loadCommandsFromFile(const char *path) {
  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  bool readingStripeBlock = false;
  Command tempCmd; 

  Serial.println("Loading Commands from File:");
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.startsWith("//") || line.length() == 0) {
      continue; 
    }

    // 1) "number of drawn columns"
    if (line.startsWith("number of drawn columns =")) {
      int eqIndex = line.indexOf('=');
      if (eqIndex >= 0) {
        String val = line.substring(eqIndex + 1);
        val.trim();
        numberOfDrawnColumns = val.toInt();
        Serial.print("Parsed number of drawn columns: ");
        Serial.println(numberOfDrawnColumns);
      }
      continue;
    }

    // 2) "pulley spacing"
    if (line.startsWith("pulley spacing =")) {
      int eqIndex = line.indexOf('=');
      if (eqIndex >= 0) {
        String val = line.substring(eqIndex + 1);
        val.trim();
        pulleySpacing = val.toFloat();
        Serial.print("Parsed pulley spacing: ");
        Serial.println(pulleySpacing);
      }
      continue;
    }

    // 3) "STRIPE - column #"
    if (line.startsWith("STRIPE - column #")) {
      readingStripeBlock = true;
      tempCmd.type        = Command::STRIPE;
      tempCmd.stripeName  = line; 
      tempCmd.drop        = 0.0;
      tempCmd.startPulleyA= 0.0;
      tempCmd.startPulleyB= 0.0;
      tempCmd.pattern     = "";
      Serial.print("Detected STRIPE block: ");
      Serial.println(tempCmd.stripeName);
      continue;
    }

    // If we are in a stripe block, parse the lines
    if (readingStripeBlock) {
      // ignore "starting/ending position pixel values"
      if (line.startsWith("starting/ending position pixel values:")) {
        continue;
      }
      // "drop:"
      if (line.startsWith("drop:")) {
        float d = line.substring(5).toFloat();
        tempCmd.drop = d;
        Serial.print("Parsed drop: ");
        Serial.println(d);
        continue;
      }
      // pattern array
      if (line.startsWith("['") && line.endsWith("]")) {
        tempCmd.pattern = line;
        Serial.println("Parsed pattern array for stripe.");
        continue;
      }
      // "starting pulley values:"
      if (line.startsWith("starting pulley values:")) {
        int colonIndex = line.indexOf(':');
        if (colonIndex >= 0) {
          String sub = line.substring(colonIndex + 1);
          sub.trim();
          int comma = sub.indexOf(',');
          if (comma >= 0) {
            String valA = sub.substring(0, comma);
            String valB = sub.substring(comma + 1);
            valA.trim();
            valB.trim();
            tempCmd.startPulleyA = valA.toFloat();
            tempCmd.startPulleyB = valB.toFloat();

            // Done reading stripe block
            commands[commandCount++] = tempCmd;
            Serial.println("Finished STRIPE command. Added to array.");
            readingStripeBlock = false;
          }
        }
        continue;
      }
    }

    // 4) If not a stripe block or recognized global line, fallback
    if (line.startsWith("change color to:")) {
        Command cmd;
        cmd.type = Command::COLOR_CHANGE;
        cmd.colorHex = line.substring(16);
        cmd.colorHex.trim();
        cmd.colorHex.toUpperCase();
        commands[commandCount++] = cmd;
    }

    else {
      // Assume (x,y) move
      int start = line.indexOf('(');
      int comma = line.indexOf(',', start);
      int end   = line.indexOf(')', comma);
      if (start == -1 || comma == -1 || end == -1) {
        continue; 
      }
      String xStr = line.substring(start + 1, comma);
      String yStr = line.substring(comma + 1, end);

      Command cmd;
      cmd.type   = Command::MOVE;
      cmd.pos.x  = xStr.toFloat();
      cmd.pos.y  = yStr.toFloat();
      commands[commandCount++] = cmd;
    }
  }
  file.close();
  Serial.print("Total commands loaded: ");
  Serial.println(commandCount);
}

// ------------------ sendTriggerCommand ----------------------------
void sendTriggerCommand() {
  if (!waitingForConfirmation) {
    commandConfirmed = false;
    waitingForConfirmation = true;
    confirmationStartTime = millis();

    outgoingMessage.command = COMMAND_RUN;
    Serial.println("Sending command to Chassis...");

    esp_err_t result = esp_now_send(chassisAddress, (uint8_t *)&outgoingMessage, sizeof(outgoingMessage));
    if (result != ESP_OK) {
      Serial.println("Error: Failed to send command. Re-adding peer.");
      esp_now_del_peer(chassisAddress);

      esp_now_peer_info_t peerInfo = {};
      memcpy(peerInfo.peer_addr, chassisAddress, 6);
      peerInfo.channel = 0;
      peerInfo.encrypt = false;
      esp_now_add_peer(&peerInfo);
    }
  }
}

// ------------------ handleSendTriggerCommand -----------------------
void handleSendTriggerCommand() {
  if (waitingForConfirmation) {
    if (commandConfirmed) {
      Serial.println("Command confirmed by Chassis");
      waitingForConfirmation = false;
      commandConfirmed        = false;
      if (runMode) {
        startNextCommand();
      }
    }
    else if (millis() - confirmationStartTime >= chassisWaitTime) {
      Serial.println("Chassis wait time exceeded. Moving on...");
      waitingForConfirmation = false;
      if (runMode) {
        startNextCommand();
      }
    }
  }
}

// ------------------ printCurrentPositions --------------------------
void printCurrentPositions() {
  float position1 = stepper1.currentPosition() / (stepsPerMeter * motor1Direction);
  float position2 = stepper2.currentPosition() / (stepsPerMeter * motor2Direction);
  Serial.print("Current Positions (m) - Motor 1: ");
  Serial.print(position1);
  Serial.print("  Motor 2: ");
  Serial.println(position2);
}
