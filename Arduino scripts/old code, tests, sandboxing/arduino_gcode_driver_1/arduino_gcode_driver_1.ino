#include <AccelStepper.h>
#include "LittleFS.h"
#include <esp_now.h>
#include <WiFi.h>

#define motorInterfaceType 1  // Using a driver that requires step and direction pins

// Define pins for Motor 1
const int stepPin1 = 5;  // Pulse pin for Motor 1
const int dirPin1 = 4;   // Direction pin for Motor 1

// Define pins for Motor 2
const int stepPin2 = 2;  // Pulse pin for Motor 2
const int dirPin2 = 1;   // Direction pin for Motor 2

bool sendTriggerAfterMovement = false;
bool runMode = false;


// variables and setup for espNOW
// Define Chassis MAC address here
uint8_t chassisAddress[] = {0x48, 0x27, 0xE2, 0xE6, 0xE8, 0x34};
bool commandConfirmed = false;
const uint8_t COMMAND_RUN = 0x01;  // Example command to send
typedef struct struct_message {
  uint8_t command;
} struct_message;
struct_message outgoingMessage;

void onDataSent(const uint8_t *macAddr, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
  delay(50);  // Small delay to ensure Serial prints completely
}
void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  Serial.println("Confirmation received from Chassis");
  commandConfirmed = true;  // Set the flag when confirmation is received
}



// Create instances of the AccelStepper class
AccelStepper stepper1(motorInterfaceType, stepPin1, dirPin1);
AccelStepper stepper2(motorInterfaceType, stepPin2, dirPin2);

// Variables for acceleration and max velocity
float acceleration = 2000.0;
float maxSpeed = 400.0;

// Conversion factor for meters to steps
const float stepsPerMeter = 200 * 16 * (.5/.15) * (50/48.4);  // Adjust this value based on your system's steps per meter

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
std::vector<Position> positions;
int currentPositionIndex = 0;  // Track the current position index for movements

// Flags
bool movementInProgress = false;
bool motor1Done = false;
bool motor2Done = false;

void setup() {
  Serial.begin(9600);
  Serial.println("Stepper Motor Control Initialized");

  // Set acceleration and max speed
  stepper1.setAcceleration(acceleration);
  stepper1.setMaxSpeed(maxSpeed);
  stepper2.setAcceleration(acceleration);
  stepper2.setMaxSpeed(maxSpeed);

  // Initialize LittleFS and load the file
  if (!LittleFS.begin()) {
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }
  loadPositionsFromFile("/gcode.txt");

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
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, chassisAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');

    if (runMode) {
      runMode = false;
      Serial.println("Run Mode Interrupted by User Input");
    }

    processSerialCommand(command);
  }

  if (movementInProgress) {
    // Existing code to run the steppers and check for completion
    bool motor1Running = (stepper1.distanceToGo() != 0);
    bool motor2Running = (stepper2.distanceToGo() != 0);

    if (motor1Running) {
      stepper1.run();
    } else if (!motor1Done) {
      motor1Done = true;
      Serial.println("Motor 1 Movement Complete");
    }

    if (motor2Running) {
      stepper2.run();
    } else if (!motor2Done) {
      motor2Done = true;
      Serial.println("Motor 2 Movement Complete");
    }

    if (motor1Done && motor2Done) {
      movementInProgress = false;
      motor1Done = false;
      motor2Done = false;
      Serial.println("Both Motors Movement Complete");

      if (runMode) {
        sendTriggerCommand();

        if (currentPositionIndex < positions.size()) {
          startNextMovement();
        } else {
          Serial.println("Run Mode Complete");
          runMode = false;
        }
      } else if (sendTriggerAfterMovement) {
        sendTriggerCommand();
        sendTriggerAfterMovement = false;
      }
    }
  }
}


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

void processSerialCommand(String command) {
  if (command == "?") {
    listAvailableCommands();
  } else if (command.startsWith("move a to ")) {
    float position = command.substring(10).toFloat();
    motor1Position = position;  // Set target position
    stepper1.moveTo(position * stepsPerMeter * motor1Direction);  // Move to position
    Serial.print("Motor 1 moving to position (m): ");
    Serial.println(position);
    movementInProgress = true;
  } else if (command.startsWith("move b to ")) {
    float position = command.substring(10).toFloat();
    motor2Position = position;  // Set target position
    stepper2.moveTo(position * stepsPerMeter * motor2Direction);  // Move to position
    Serial.print("Motor 2 moving to position (m): ");
    Serial.println(position);
    movementInProgress = true;
  } else if (command == "zero a") {
    motor1Position = 0.0;
    stepper1.setCurrentPosition(0);
    Serial.println("Motor 1 zeroed");
  } else if (command == "zero b") {
    motor2Position = 0.0;
    stepper2.setCurrentPosition(0);
    Serial.println("Motor 2 zeroed");
  } else if (command == "go") {
    startNextMovement();
    sendTriggerAfterMovement = true;  // Set flag to send trigger after movement
  } else if (command == "run") {
    startRunMode();
  }
}

void startRunMode() {
  Serial.println("Starting Run Mode");
  runMode = true;
  currentPositionIndex = 0;  // Reset to the first position
  startNextMovement();
}

void listAvailableCommands() {
  Serial.println("Available Commands:");
  Serial.println("  go           - Start the next movement in sequence");
  Serial.println("  move a to X  - Move Motor 1 to the specified position X in meters");
  Serial.println("  move b to X  - Move Motor 2 to the specified position X in meters");
  Serial.println("  zero a       - Set Motor 1 position to zero");
  Serial.println("  zero b       - Set Motor 2 position to zero");
  Serial.println("  ?            - List all available commands");
}

void startNextMovement() {
  if (currentPositionIndex >= positions.size()) {
    Serial.println("All Movements Complete");
    stepper1.disableOutputs();
    stepper2.disableOutputs();
    while (1);  // Halt the program
  }

  Position pos = positions[currentPositionIndex];
  move_to_position(pos.x, pos.y);

  Serial.print("Starting Movement ");
  Serial.println(currentPositionIndex + 1);
  currentPositionIndex++;
}

void loadPositionsFromFile(const char* path) {
  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.println("Loading Positions from File:");
  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (line.startsWith("//")) continue;  // Skip comment lines

    int start = line.indexOf('(');
    int comma = line.indexOf(',', start);
    int end = line.indexOf(')', comma);

    if (start == -1 || comma == -1 || end == -1) continue;

    String xStr = line.substring(start + 1, comma);
    String yStr = line.substring(comma + 1, end);

    Position pos;
    pos.x = xStr.toFloat();
    pos.y = yStr.toFloat();

    positions.push_back(pos);
  }
  file.close();
}

// Function to send the trigger command
void sendTriggerCommand() {
  commandConfirmed = false;  // Reset confirmation flag

  // Prepare and send command to Chassis
  outgoingMessage.command = COMMAND_RUN;
  Serial.println("Sending command to Chassis...");
  
  esp_err_t result = esp_now_send(chassisAddress, (uint8_t *) &outgoingMessage, sizeof(outgoingMessage));

  if (result != ESP_OK) {
    Serial.println("Error: Failed to send command. Re-adding peer.");
    esp_now_del_peer(chassisAddress);
    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, chassisAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
  }

  // Wait for confirmation with a timeout of 3 seconds
  unsigned long startTime = millis();
  while (!commandConfirmed && millis() - startTime < 3000) {  // 3-second timeout
    delay(100);  // Short delay while waiting for response
  }

  if (commandConfirmed) {
    Serial.println("Command confirmed by Chassis");
  } else {
    Serial.println("No response from Chassis. Moving on...");
  }
}