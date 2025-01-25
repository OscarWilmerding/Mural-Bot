// Include necessary libraries
#include <AccelStepper.h>
#include "LittleFS.h"
#include <esp_now.h>
#include <WiFi.h>

// Define motor interface type
#define motorInterfaceType 1  // Using a driver that requires step and direction pins

// Define pins for Motor 1 (now using Motor 2's original pins)
const int stepPin1 = 7;  // Pulse pin for Motor 1
const int dirPin1 = 8;   // Direction pin for Motor 1

// Define pins for Motor 2 (now using Motor 1's original pins)
const int stepPin2 = 5;  // Pulse pin for Motor 2
const int dirPin2 = 4;   // Direction pin for Motor 2

// Motor control variables
bool sendTriggerAfterMovement = false;
bool runMode = false;
bool movementInProgress = false;
bool motor1Done = false;
bool motor2Done = false;

// ESP-NOW variables
uint8_t chassisAddress[] = { 0x48, 0x27, 0xE2, 0xE6, 0xE8, 0x34 };
bool commandConfirmed = false;
const uint8_t COMMAND_RUN = 0x01;
typedef struct struct_message {
  uint8_t command;
} struct_message;
struct_message outgoingMessage;

// Confirmation handling variables
bool waitingForConfirmation = false;
unsigned long confirmationStartTime = 0;
unsigned long confirmationTimeout = 5000;  // Default 5 seconds; adjustable as needed

// Create instances of the AccelStepper class
AccelStepper stepper1(motorInterfaceType, stepPin1, dirPin1);
AccelStepper stepper2(motorInterfaceType, stepPin2, dirPin2);

// Variables for acceleration and max velocity
float baseAcceleration = 2000.0*2.0;
float baseMaxSpeed = 400.0*2.0;
float accelerationMultiplier = 1.0;
float maxSpeedMultiplier = 1.0;

// Conversion factor for meters to steps
const float stepsPerMeter = 10494.55 *2.0;  // Use floating-point division

// Direction control variables
int motor1Direction = -1;  // Set to -1 to reverse Motor 1 direction
int motor2Direction = -1;  // Set to -1 to reverse Motor 2 direction

// Motor positions in meters
float motor1Position = 0.0;
float motor2Position = 0.0;

// Position structure and variables
struct Position {
  float x;
  float y;
};

struct Command {
  enum Type { MOVE,
              COLOR_CHANGE } type;
  Position pos;     // For MOVE commands
  String colorHex;  // For COLOR_CHANGE commands
};

// Fixed-size array for commands
const int MAX_COMMANDS = 8000;  // Adjust as needed based on available memory
Command commands[MAX_COMMANDS];
int commandCount = 0;
int currentCommandIndex = 0;

// Pause duration before sending trigger command (in milliseconds)
unsigned long prePokePause = 0;

// Chassis wait time after sending trigger command (in milliseconds)
unsigned long chassisWaitTime = 2500;  // Default to 5000 ms

// Function prototypes
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

// Setup function
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

  // Register sending and receiving callbacks
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  // Register peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, chassisAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

// Main loop function
void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');

    if (runMode) {
      runMode = false;
      Serial.println("Run Mode Interrupted by User Input");
    }

    processSerialCommand(command);

    Serial.println();  // Add a line break after each command is processed
  }

  // Always call stepper run methods
  stepper1.run();
  stepper2.run();

  // Existing code to check movement completion
  if (movementInProgress) {
    // Check if motors have reached their positions
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
      motor1Done = false;
      motor2Done = false;
      Serial.println("Both Motors Movement Complete");

      printCurrentPositions();  // Print positions after movement is complete

      if (prePokePause > 0) {
        Serial.print("Waiting for pre-poke pause: ");
        Serial.print(prePokePause);
        Serial.println(" ms");
        delay(prePokePause);
        Serial.println("Pre-poke pause completed.");
      }

      if (runMode || sendTriggerAfterMovement) {
        sendTriggerCommand();
        sendTriggerAfterMovement = false;  // Reset the flag if it was set
      }

      Serial.println();  // Add a line break after movement is complete
    }
  }

  // Handle sendTriggerCommand timeout and confirmation
  handleSendTriggerCommand();
}

// Callback when data is sent
void onDataSent(const uint8_t *macAddr, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
  delay(50);  // Small delay to ensure Serial prints completely
}

// Callback when data is received
void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  Serial.println("Confirmation received from Chassis");
  commandConfirmed = true;  // Set the flag when confirmation is received
}

// Function to move to specified positions
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

// Function to process serial commands
void processSerialCommand(String command) {
  command.trim();  // Remove any leading/trailing whitespace
  if (command == "?") {
    listAvailableCommands();
  } else if (command.startsWith("move a to ")) {
    float position = command.substring(10).toFloat();
    motor1Position = position;                                    // Set target position
    stepper1.moveTo(position * stepsPerMeter * motor1Direction);  // Move to position
    Serial.print("Motor 1 moving to position (m): ");
    Serial.println(position);
    movementInProgress = true;
  } else if (command.startsWith("move b to ")) {
    float position = command.substring(10).toFloat();
    motor2Position = position;                                    // Set target position
    stepper2.moveTo(position * stepsPerMeter * motor2Direction);  // Move to position
    Serial.print("Motor 2 moving to position (m): ");
    Serial.println(position);
    movementInProgress = true;
  } else if (command.startsWith("move a ")) {
    float distance = command.substring(7).toFloat();
    stepper1.move(distance * stepsPerMeter * motor1Direction);  // Relative move
    Serial.print("Motor 1 moving by (m): ");
    Serial.println(distance);
    movementInProgress = true;
  } else if (command.startsWith("move b ")) {
    float distance = command.substring(7).toFloat();
    stepper2.move(distance * stepsPerMeter * motor2Direction);  // Relative move
    Serial.print("Motor 2 moving by (m): ");
    Serial.println(distance);
    movementInProgress = true;
  } else if (command.startsWith("set a to ")) {
    float position = command.substring(9).toFloat();
    motor1Position = position;
    stepper1.setCurrentPosition(position * stepsPerMeter * motor1Direction);
    Serial.print("Motor 1 position set to (m): ");
    Serial.println(position);
  } else if (command.startsWith("set b to ")) {
    float position = command.substring(9).toFloat();
    motor2Position = position;
    stepper2.setCurrentPosition(position * stepsPerMeter * motor2Direction);
    Serial.print("Motor 2 position set to (m): ");
    Serial.println(position);
  } else if (command.startsWith("acceleration multiplier ")) {
    float multiplier = command.substring(23).toFloat();
    float previousAcceleration = baseAcceleration * accelerationMultiplier;
    accelerationMultiplier = multiplier;
    float newAcceleration = baseAcceleration * accelerationMultiplier;
    stepper1.setAcceleration(newAcceleration);
    stepper2.setAcceleration(newAcceleration);
    Serial.print("Acceleration multiplier set to: ");
    Serial.println(multiplier);
    Serial.print("Previous acceleration (units): ");
    Serial.println(previousAcceleration);
    Serial.print("New acceleration (units): ");
    Serial.println(newAcceleration);
  } else if (command.startsWith("velocity multiplier ")) {
    float multiplier = command.substring(19).toFloat();
    float previousMaxSpeed = baseMaxSpeed * maxSpeedMultiplier;
    maxSpeedMultiplier = multiplier;
    float newMaxSpeed = baseMaxSpeed * maxSpeedMultiplier;
    stepper1.setMaxSpeed(newMaxSpeed);
    stepper2.setMaxSpeed(newMaxSpeed);
    Serial.print("Velocity multiplier set to: ");
    Serial.println(multiplier);
    Serial.print("Previous max speed (units): ");
    Serial.println(previousMaxSpeed);
    Serial.print("New max speed (units): ");
    Serial.println(newMaxSpeed);
  } else if (command == "zero a") {
    motor1Position = 0.0;
    stepper1.setCurrentPosition(0);
    Serial.println("Motor 1 zeroed");
  } else if (command == "zero b") {
    motor2Position = 0.0;
    stepper2.setCurrentPosition(0);
    Serial.println("Motor 2 zeroed");
  } else if (command == "go") {
    startNextCommand();
    sendTriggerAfterMovement = true;  // Set flag to send trigger after movement
  } else if (command == "run") {
    runMode = true;
    startNextCommand();
  } else if (command.startsWith("set confirmation timeout ")) {
    // Allow user to adjust the confirmation timeout
    unsigned long newTimeout = command.substring(24).toInt();
    confirmationTimeout = newTimeout;
    Serial.print("Confirmation timeout set to: ");
    Serial.print(confirmationTimeout);
    Serial.println(" ms");
  } else if (command == "restart") {
    Serial.println("Restarting ESP32...");
    ESP.restart();
  } else if (command == "reset run") {
    currentCommandIndex = 0;
    Serial.println("Run index reset. Ready to run from the beginning.");
  } else if (command.startsWith("pre poke pause ")) {
    unsigned long pauseDuration = command.substring(15).toInt();
    prePokePause = pauseDuration;
    Serial.print("Pre-poke pause set to: ");
    Serial.print(prePokePause);
    Serial.println(" ms");
  } else if (command.startsWith("chasseyWaitTime ")) {
    unsigned long waitTime = command.substring(15).toInt();
    chassisWaitTime = waitTime;
    Serial.print("Chassis wait time set to: ");
    Serial.print(chassisWaitTime);
    Serial.println(" ms");
  
  } else if (command == "skip color") {
  // Store the starting index
  int originalIndex = currentCommandIndex;

  // Advance currentCommandIndex until we hit a COLOR_CHANGE command or run out of commands
  while (currentCommandIndex < commandCount && commands[currentCommandIndex].type != Command::COLOR_CHANGE) {
    currentCommandIndex++;
  }

  if (currentCommandIndex >= commandCount) {
    Serial.println("No further COLOR_CHANGE commands found. All remaining moves skipped.");
  } else {
    Serial.print("Skipped all MOVE commands from index ");
    Serial.print(originalIndex + 1);
    Serial.print(" to next COLOR_CHANGE at index ");
    Serial.println(currentCommandIndex + 1);
  }

  // Print current positions after skipping
  printCurrentPositions();

  } else if (command == "test") {
  Serial.println("Triggering chassis without movement...");
  sendTriggerCommand();

  // Print current positions after sending test trigger
  printCurrentPositions();
  } else if (command.startsWith("set command index ")) {
  int newIndex = command.substring(18).toInt();
  if (newIndex >= 0 && newIndex < commandCount) {
    currentCommandIndex = newIndex;
    Serial.print("Command index set to: ");
    Serial.println(currentCommandIndex);
  } else {
    Serial.println("Invalid index. Please choose a value within the loaded command range.");
  }
  printCurrentPositions();

  } else {
    Serial.println("Invalid command. Type '?' for a list of commands.");
  }

  // If not starting a movement, print current positions
  if (!movementInProgress) {
    printCurrentPositions();
  }
}

// Function to start the next command
void startNextCommand() {
  if (currentCommandIndex < commandCount) {
    Command cmd = commands[currentCommandIndex];

    if (cmd.type == Command::MOVE) {
      move_to_position(cmd.pos.x, cmd.pos.y);
      Serial.print("Starting Movement ");
      Serial.println(currentCommandIndex + 1);
      currentCommandIndex++;
    } else if (cmd.type == Command::COLOR_CHANGE) {
      Serial.print("COLOR CHANGE TO ");
      Serial.println(cmd.colorHex);
      runMode = false;       // Pause the run command
      currentCommandIndex++; // Move to the next command
      // Wait for 'run' or 'go' command to proceed
    }
  } else {
    Serial.println("All Commands Complete");
    stepper1.disableOutputs();
    stepper2.disableOutputs();
    runMode = false;
  }
}

// Function to list available commands
void listAvailableCommands() {
  Serial.println("Available Commands:");
  Serial.println("  go                          - Start the next command in sequence");
  Serial.println("  run                         - Run all commands from current position");
  Serial.println("  move a to X                 - Move Motor 1 to position X meters (absolute)");
  Serial.println("  move b to X                 - Move Motor 2 to position X meters (absolute)");
  Serial.println("  move a X                    - Move Motor 1 by X meters (relative)");
  Serial.println("  move b X                    - Move Motor 2 by X meters (relative)");
  Serial.println("  zero a                      - Set Motor 1 position to zero");
  Serial.println("  zero b                      - Set Motor 2 position to zero");
  Serial.println("  set a to X                  - Set Motor 1 current position to X meters");
  Serial.println("  set b to X                  - Set Motor 2 current position to X meters");
  Serial.println("  acceleration multiplier X   - Set acceleration multiplier to X");
  Serial.println("  velocity multiplier X       - Set velocity multiplier to X");
  Serial.println("  set confirmation timeout X  - Set confirmation timeout to X milliseconds");
  Serial.println("  restart                     - Restart the ESP32");
  Serial.println("  reset run                   - Reset the run index to start from beginning");
  Serial.println("  pre poke pause X            - Set pause duration before sending trigger command to X milliseconds");
  Serial.println("  chasseyWaitTime X           - Set chassis wait time to X milliseconds");
  Serial.println("  skip color                  - Skip all MOVE commands until the next COLOR_CHANGE command");
  Serial.println("  test                        - Trigger the chassis command without any movement");
  Serial.println("  set command index X         - Set the current command index to X (0-based)");
  Serial.println("  ?                           - List all available commands");
}

// Function to load commands from file
void loadCommandsFromFile(const char *path) {
  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.println("Loading Commands from File:");
  while (file.available() && commandCount < MAX_COMMANDS) {
    String line = file.readStringUntil('\n');
    line.trim();  // Remove any leading/trailing whitespace
    if (line.startsWith("//") || line.length() == 0)
      continue;  // Skip comment lines and empty lines

    if (line.startsWith("change color to:")) {
      Command cmd;
      cmd.type = Command::COLOR_CHANGE;
      cmd.colorHex = line.substring(16);  // Extract the hex code
      cmd.colorHex.toUpperCase();
      commands[commandCount++] = cmd;

      // Print update every 100 commands
      if (commandCount % 100 == 0) {
        Serial.print("Loaded Command ");
        Serial.print(commandCount);
        Serial.println("...");
      }

    } else {
      int start = line.indexOf('(');
      int comma = line.indexOf(',', start);
      int end = line.indexOf(')', comma);

      if (start == -1 || comma == -1 || end == -1)
        continue;

      String xStr = line.substring(start + 1, comma);
      String yStr = line.substring(comma + 1, end);

      Command cmd;
      cmd.type = Command::MOVE;
      cmd.pos.x = xStr.toFloat();
      cmd.pos.y = yStr.toFloat();
      commands[commandCount++] = cmd;

      // Print update every 100 commands
      if (commandCount % 100 == 0) {
        Serial.print("Loaded Command ");
        Serial.print(commandCount);
        Serial.println("...");
      }
    }
  }
  file.close();
  Serial.print("Total commands loaded: ");
  Serial.println(commandCount);
}

// Function to send trigger command
void sendTriggerCommand() {
  if (!waitingForConfirmation) {
    commandConfirmed = false;  // Reset confirmation flag
    waitingForConfirmation = true;
    confirmationStartTime = millis();

    // Prepare and send command to Chassis
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

// Function to handle sendTriggerCommand confirmation and chassis wait time
void handleSendTriggerCommand() {
  if (waitingForConfirmation) {
    if (commandConfirmed) {
      Serial.println("Command confirmed by Chassis");
      waitingForConfirmation = false;
      commandConfirmed = false;  // Reset the flag
      if (runMode) {
        startNextCommand();
      }
    } else if (millis() - confirmationStartTime >= chassisWaitTime) {
      Serial.println("Chassis wait time exceeded. Moving on...");
      waitingForConfirmation = false;
      if (runMode) {
        startNextCommand();
      }
    }
  }
}

// Function to print current positions in meters
void printCurrentPositions() {
  float position1 = (stepper1.currentPosition() / (stepsPerMeter * motor1Direction));
  float position2 = (stepper2.currentPosition() / (stepsPerMeter * motor2Direction));
  Serial.print("Current Positions (m) - Motor 1: ");
  Serial.print(position1);
  Serial.print(" Motor 2: ");
  Serial.println(position2);
}
