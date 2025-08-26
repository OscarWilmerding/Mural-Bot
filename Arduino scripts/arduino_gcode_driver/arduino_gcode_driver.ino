/************************************************************/
/*                   INCLUDE & DEPENDENCIES                 */
/************************************************************/
#include <AccelStepper.h>
#include "LittleFS.h"
#include <esp_now.h>
#include <WiFi.h>


/************************************************************/
/*                   MOTOR & PIN DEFINITIONS                */
/************************************************************/

const int redButtonPin = 2;
const int greyButtonPin = 9;

// Using a driver that requires step and direction pins
#define motorInterfaceType 1

// Define pins for Motors
// IF YOU ARE GOING TO CHANGE THESE YOU PROBABLY WIRED IT WRONG
const int stepPin1 = 5;
const int dirPin1  = 4;
const int stepPin2 = 7;
const int dirPin2  = 8;

// Create instances of the AccelStepper class
AccelStepper stepper1(motorInterfaceType, stepPin1, dirPin1);
AccelStepper stepper2(motorInterfaceType, stepPin2, dirPin2);


/************************************************************/
/*                       GLOBAL VARIABLES                   */
/************************************************************/ 
bool sendTriggerAfterMovement   = false;
bool runMode                    = false;
bool movementInProgress         = false;
bool motor1Done                 = false;
bool motor2Done                 = false;

// For acceleration and max velocity
float baseAcceleration          = 2000.0 * 2.0;
float baseMaxSpeed              = 400.0  * 2.0;
float accelerationMultiplier    = 1.0;
float maxSpeedMultiplier        = 1.0;

// ESP-NOW variables
uint8_t chassisAddress[]        = { 0xA0, 0xB7, 0x65, 0x07, 0xD5, 0x78 };
bool commandConfirmed           = false;
const uint8_t COMMAND_RUN       = 0x01;

typedef struct struct_message {
  uint8_t command;
  uint8_t chunkIndex;
} struct_message;

struct_message outgoingMessage;
struct_message incomingMessage; 

bool finalAckReceived = false;
bool          waitingForConfirmation = false;
unsigned long confirmationStartTime   = 0;
unsigned long confirmationTimeout     = 5000;  // Default 5s timeout waiting for response

// Conversion factor (meters -> steps)
// this number has had many calibration constants applied to it which is what makes it ugly. derived via trial and error.
// if its overshooting this number should become SMALLER
float stepsPerMeter       = 9727;  

// Direction control variab
int motor1Direction             = -1;
int motor2Direction             = -1;

// Store motor positions in meters
float motor1Position            = 0.0;
float motor2Position            = 0.0;

// Pre/post movement timing
unsigned long prePokePause      = 0;
unsigned long chassisWaitTime   = 2500;

// Additional data from G-code
float pulleySpacing             = 0.0;
int   numberOfDrawnColumns      = 0;
float stripeVelocity            = 0.05;

// New global for controlling how often (ms) you recalc velocities
unsigned long velocityCalcDelay = 100; 

// below vars relating to long string sending
static const int CHUNK_PAYLOAD_SIZE = 200; 
static bool     largeStringInProgress = false;
static String   largeStringToSend     = "";
static int      totalChunks           = 0;
static int      currentChunkIndex     = 0;
// Retry logic
static bool     largeStringAckReceived = false;
static unsigned long lastSendAttempt   = 0;
static unsigned long resendDelay       = 5000; 

// HANDSHAKE STATE
volatile bool startAckReceived = false;


/************************************************************/
/*                       DATA STRUCTURES                    */
/************************************************************/
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


/************************************************************/
/*                COMMAND STORAGE & MANAGEMENT              */
/************************************************************/
// NOTE: This number is important. If there is a memory overflow,
// lower this number because the ESP32 can only manage so many 
// global variables at once.
const int MAX_COMMANDS      = 200; 
Command commands[MAX_COMMANDS];
int     commandCount        = 0;
int     currentCommandIndex = 0;


/************************************************************/
/*                  FUNCTION DECLARATIONS                   */
/************************************************************/
void onDataSent(const esp_now_send_info_t *info, esp_now_send_status_t status);
void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len);
void move_to_position(float position1, float position2);
void move_to_position_blocking(float position1, float position2);
void processSerialCommand(String command);
void startNextCommand();
void listAvailableCommands();
void loadCommandsFromFile(const char *path);
void sendTriggerCommand();
void handleSendTriggerCommand();
void printCurrentPositions();
void determineStripeVelocities(float posA, float posB, float &velA, float &velB);
void startLargeStringSend(const String &strToSend);
void sendNextChunk();
void IRAM_ATTR handleResetInterrupt();
static void sendStartPacket();   // forward declaration for start-ack packet


/************************************************************/
/*                   SETUP FUNCTION                         */
/************************************************************/
void setup() {
  Serial.begin(115200);
  delay(5000); // arduino connects to serial slow as hell so this is necissary
  Serial.println("Stepper Motor Control Initialized");
  // Set acceleration and max speed
  stepper1.setAcceleration(baseAcceleration * accelerationMultiplier);
  stepper1.setMaxSpeed(baseMaxSpeed * maxSpeedMultiplier);
  stepper2.setAcceleration(baseAcceleration * accelerationMultiplier);
  stepper2.setMaxSpeed(baseMaxSpeed * maxSpeedMultiplier);
  
  pinMode(redButtonPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(redButtonPin), handleResetInterrupt, FALLING);
  pinMode(greyButtonPin, INPUT_PULLUP);

  // Initialize LittleFS and load the file
  if (!LittleFS.begin()) {
    Serial.println("An error has occurred while mounting LittleFS");
    return;
  }
  loadCommandsFromFile("/gcode.txt");

  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  delay(1000); // to get right reading
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());
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


/************************************************************/
/*                     MAIN LOOP                            */
/************************************************************/
void loop() {
  // Check Serial for user command

  if (digitalRead(greyButtonPin) == LOW) {
    Serial.println("grey button pressed");
    processSerialCommand("go");
  }

  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim(); // Ensure no extra spaces or newlines

    Serial.print("USER COMMAND: ");
    Serial.println(command); // Print the command entered before processing

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

  if (largeStringInProgress && !largeStringAckReceived) {
    // Check if we still have chunks left to send
    static unsigned long chunkSendInterval = 50; // e.g., 50ms between chunk sends
    static unsigned long lastChunkSendTime = 0;

    if (millis() - lastChunkSendTime > chunkSendInterval && currentChunkIndex < totalChunks) {
      sendNextChunk();
      lastChunkSendTime = millis();
    }

    // Check for confirmation timeout
    if (millis() - lastSendAttempt > resendDelay && currentChunkIndex >= totalChunks) {
      // If we haven't received confirmation for 5s after sending all chunks, retry
      Serial.println("No confirmation received. Retrying large string transmission...");
      // Reset index, re-send everything
      currentChunkIndex = 0;
      lastSendAttempt   = millis();
      sendNextChunk();
    }
  }

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

/*****************************************************************************/
/*                   2) startLargeStringSend() Function                      */
/*****************************************************************************/
void startLargeStringSend(const String &strToSend) {
  largeStringToSend      = strToSend;
  int len                = largeStringToSend.length();
  totalChunks            = (len + CHUNK_PAYLOAD_SIZE - 1) / CHUNK_PAYLOAD_SIZE;
  currentChunkIndex      = 0;
  largeStringInProgress  = true;
  largeStringAckReceived = false;
  finalAckReceived       = false;
  startAckReceived       = false;

  Serial.println("Initiating large string send...");
  sendStartPacket();

  // wait for start-ack with retries
  unsigned long t0 = millis();
  const unsigned long startTimeout = 1000;
  while (!startAckReceived) {
    if (millis() - t0 > startTimeout) {
      Serial.println("No start-ack, resending 0x10...");
      sendStartPacket();
      t0 = millis();
    }
    delay(10);
  }
  Serial.println("Start-ack received.");

  for (currentChunkIndex = 0; currentChunkIndex < totalChunks; ) {
    largeStringAckReceived = false;
    sendNextChunk();

    unsigned long sendTime = millis();
    const unsigned long timeout = 500; // per-chunk timeout

    while (!largeStringAckReceived) {
      if (millis() - sendTime > timeout) {
        // if stuck on chunk 0, also resend 0x10
        if (currentChunkIndex == 0) sendStartPacket();
        Serial.println("Chunk-ack timeout, resending chunk...");
        sendNextChunk();
        sendTime = millis();
      }
      delay(10);
    }
  }

  // wait for final 0x12
  unsigned long finalT = millis();
  while (!finalAckReceived && millis() - finalT < 3000) delay(10);

  largeStringInProgress = false;
  Serial.println(finalAckReceived ? "Entire large string confirmed." : "Timed out waiting for final ack.");
}

/*****************************************************************************/
/*                        3) sendNextChunk() Function                        */
/*****************************************************************************/
void sendNextChunk() {
  if (!largeStringInProgress) return;  
  if (currentChunkIndex >= totalChunks) {
    Serial.println("All chunks sent. Waiting for confirmation...");
    return;
  }

  // Prepare chunk
  int startIdx = currentChunkIndex * CHUNK_PAYLOAD_SIZE;
  String chunkData = largeStringToSend.substring(startIdx, startIdx + CHUNK_PAYLOAD_SIZE);

  // Build packet
size_t packetSize = 2 + chunkData.length();          // no +1
uint8_t *packet   = (uint8_t *) malloc(packetSize);
packet[0] = 0x11;
packet[1] = (uint8_t) currentChunkIndex;
memcpy(&packet[2], chunkData.c_str(), chunkData.length());   // no +1

  // Send
  esp_err_t result = esp_now_send(chassisAddress, packet, packetSize);
  free(packet);

  if (result == ESP_OK) {
    Serial.print("Sent chunk #");
    Serial.print(currentChunkIndex);
    Serial.print(" of ");
    Serial.println(totalChunks);
  } else {
    Serial.print("Failed to send chunk #");
    Serial.println(currentChunkIndex);
  }

  // increment only after ack in onDataRecv
}


/************************************************************/
/*             CALLBACKS: ESP-NOW DATA SENT/RECV            */
/************************************************************/
void onDataSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  if (len == sizeof(struct_message)) {
    memcpy(&incomingMessage, incomingData, sizeof(incomingMessage));
    if (incomingMessage.command == 0x10) {      // start-ack
      startAckReceived = true;
    } else if (incomingMessage.command == 0x13) {
      if (incomingMessage.chunkIndex == currentChunkIndex) {
        if (!largeStringAckReceived) {
          largeStringAckReceived = true;
          currentChunkIndex++;
        }
      }
    } else if (incomingMessage.command == 0x12) {
      finalAckReceived = true;
    }
  }
}

static void sendStartPacket() {
  uint8_t startPacket[3] = {
    0x10,
    (uint8_t)((totalChunks >> 8) & 0xFF),
    (uint8_t)(totalChunks & 0xFF)
  };
  esp_err_t result = esp_now_send(chassisAddress, startPacket, sizeof(startPacket));
  if (result != ESP_OK) {
    Serial.println("Failed to send start packet");
  }
}


/************************************************************/
/*                  MOVEMENT HELPER FUNCTIONS               */
/************************************************************/
// this function needs a .run command to be called frequently after this is ran
void move_to_position(float position1, float position2) {
  long targetPosition1 = position1 * stepsPerMeter * motor1Direction;
  long targetPosition2 = position2 * stepsPerMeter * motor2Direction;

  stepper1.setAcceleration(baseAcceleration * accelerationMultiplier);
  stepper2.setAcceleration(baseAcceleration * accelerationMultiplier);
  // don't remove this -- it gets messed with in velocity calc
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

// this function is similar but does not require .run to be called constantly
void move_to_position_blocking(float position1, float position2) {
  // Convert meters to steps, applying direction
  long targetPosition1 = position1 * stepsPerMeter * motor1Direction;
  long targetPosition2 = position2 * stepsPerMeter * motor2Direction;

  // Optionally override accel/speed here if desired
  stepper1.setAcceleration(baseAcceleration * accelerationMultiplier);
  stepper2.setAcceleration(baseAcceleration * accelerationMultiplier);
  stepper1.setMaxSpeed(baseMaxSpeed * maxSpeedMultiplier);
  stepper2.setMaxSpeed(baseMaxSpeed * maxSpeedMultiplier);

  // Set the new targets
  stepper1.moveTo(targetPosition1);
  stepper2.moveTo(targetPosition2);

  // Print for debugging
  Serial.print("Blocking move to (m):  A=");
  Serial.print(position1);
  Serial.print("  B=");
  Serial.println(position2);

  // Run until both steppers reach their targets
  while ((stepper1.distanceToGo() != 0) || (stepper2.distanceToGo() != 0)) {
    stepper1.run();
    stepper2.run();
  }

  Serial.println("Blocking move complete.");
}


/************************************************************/
/*            SERIAL COMMAND PROCESSING FUNCTION            */
/************************************************************/
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
  else if (command.startsWith("spr ")) {          // e.g. “SPR 3200”
    int newSPR = command.substring(4).toInt();    // grab the number after the space
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


/************************************************************/
/*        FUNCTION: START NEXT COMMAND IN COMMAND LIST      */
/************************************************************/
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

      // JSON formatting of stripe data
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

      // Move to initial stripe position...
      Serial.println("Moving to initial stripe position...");
      move_to_position_blocking(cmd.startPulleyA, cmd.startPulleyB);

      stepper1.setAcceleration(0);
      stepper2.setAcceleration(0);
      stepper1.setMaxSpeed(8000);
      stepper2.setMaxSpeed(8000);

      Serial.println("Done moving to initial position.");
      delay(2000);

      // Send entire JSON data string using large string send scheme
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
    }

  }
  else {
    Serial.println("All Commands Complete");
    stepper1.disableOutputs();
    stepper2.disableOutputs();
    runMode = false;
  }
}



/************************************************************/
/*        LIST AVAILABLE SERIAL CONSOLE COMMANDS            */
/************************************************************/
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
  Serial.println("  ?                           - Show this help list");
}


/************************************************************/
/*          LOAD COMMANDS FROM FILE (LittleFS)              */
/************************************************************/
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

    // Skip commented or empty lines
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
      tempCmd.type         = Command::STRIPE;
      tempCmd.stripeName   = line;  // e.g. "STRIPE - column #0"
      tempCmd.drop         = 0.0;
      tempCmd.startPulleyA = 0.0;
      tempCmd.startPulleyB = 0.0;
      tempCmd.pattern      = "";
      Serial.print("Detected STRIPE block: ");
      Serial.println(tempCmd.stripeName);
      continue;
    }

    // If we are in a stripe block, parse any relevant lines
    if (readingStripeBlock) {
      // Ignore the "starting/ending position pixel values" line
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
      // "pattern:"
      if (line.startsWith("pattern:")) {
        int colonIndex = line.indexOf(':');
        if (colonIndex >= 0) {
          String patternData = line.substring(colonIndex + 1);
          patternData.trim();
          tempCmd.pattern = patternData;
          Serial.println("Parsed pattern array for stripe.");
          Serial.println("pattern data raw being loaded into command struct:" + tempCmd.pattern);
        }
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

            // Done reading this stripe block
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
      Serial.print("UNRECOGNIZED COMMANDS IN GCODE FILE");
    }
  }

  file.close();
  Serial.print("Total commands loaded: ");
  Serial.println(commandCount);
}


/************************************************************/
/*          SEND TRIGGER COMMAND TO CHASSIS (ESP-NOW)       */
/************************************************************/
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


/************************************************************/
/*      HANDLE TRIGGER COMMAND CONFIRMATION/TIMEOUT         */
/************************************************************/
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


/************************************************************/
/*  PRINT CURRENT POSITION (METERS & STEPS) FOR DEBUGGING   */
/************************************************************/
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


/************************************************************/
/*   DETERMINE STRIPE VELOCITIES (CUSTOM CALC FOR STRIPES)  */
/************************************************************/
void determineStripeVelocities(float posA, float posB, float &velA, float &velB) {
  float Vx = 0; // no movement in x direction.
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

void IRAM_ATTR handleResetInterrupt() {
  Serial.println("Red button pressed, restarting esp32");
  esp_restart(); // Soft reset the ESP32
}
