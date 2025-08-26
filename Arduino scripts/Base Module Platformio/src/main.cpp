#include <Arduino.h>
#include <AccelStepper.h>
#include "LittleFS.h"
#include <esp_now.h>
#include <WiFi.h>

#include "state.h"
#include "modules.h"

/************************************************************/
/*                   MOTOR & PIN DEFINITIONS                */
/************************************************************/
const int redButtonPin  = 2;
const int greyButtonPin = 9;

// Using a driver that requires step and direction pins
#define motorInterfaceType 1

// Define pins for Motors
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

struct_message outgoingMessage;
struct_message incomingMessage;

bool          finalAckReceived        = false;
bool          waitingForConfirmation  = false;
unsigned long confirmationStartTime   = 0;
unsigned long confirmationTimeout     = 5000;  // Default 5s

// meters -> steps
float stepsPerMeter       = 9727;

// Motor directions
int motor1Direction       = -1;
int motor2Direction       = -1;

// Positions (meters)
float motor1Position      = 0.0;
float motor2Position      = 0.0;

// Timing knobs
unsigned long prePokePause    = 0;
unsigned long chassisWaitTime = 2500;

// From G-code
float pulleySpacing        = 0.0;
int   numberOfDrawnColumns = 0;
float stripeVelocity       = 0.05;

// Velocity recalc cadence
unsigned long velocityCalcDelay = 100;

// Large string sending
const int      CHUNK_PAYLOAD_SIZE     = 200;
bool           largeStringInProgress  = false;
String         largeStringToSend      = "";
int            totalChunks            = 0;
int            currentChunkIndex      = 0;
bool           largeStringAckReceived = false;
unsigned long  lastSendAttempt        = 0;
unsigned long  resendDelay            = 5000;

// Handshake state
volatile bool startAckReceived = false;

/************************************************************/
/*                COMMAND STORAGE & MANAGEMENT              */
/************************************************************/
Command commands[MAX_COMMANDS];
int     commandCount        = 0;
int     currentCommandIndex = 0;

/************************************************************/
/*                   SETUP FUNCTION                         */
/************************************************************/
void setup() {
  Serial.begin(115200);
  delay(5000);
  Serial.println("Stepper Motor Control Initialized");

  // Stepper tuning
  stepper1.setAcceleration(baseAcceleration * accelerationMultiplier);
  stepper1.setMaxSpeed(baseMaxSpeed * maxSpeedMultiplier);
  stepper2.setAcceleration(baseAcceleration * accelerationMultiplier);
  stepper2.setMaxSpeed(baseMaxSpeed * maxSpeedMultiplier);

  pinMode(redButtonPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(redButtonPin), handleResetInterrupt, FALLING);
  pinMode(greyButtonPin, INPUT_PULLUP);

  // FS
  if (!LittleFS.begin()) {
    Serial.println("An error has occurred while mounting LittleFS");
    return;
  }
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);
  Serial.println(WiFi.macAddress());
  Serial.println("ESP-NOW Hub Initialized");

  // ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

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
  if (digitalRead(greyButtonPin) == LOW) {
    Serial.println("grey button pressed");
    processSerialCommand("go");
  }

  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    Serial.print("USER COMMAND: ");
    Serial.println(command);

    if (runMode) {
      runMode = false;
      Serial.println("Run Mode Interrupted by User Input");
    }
    processSerialCommand(command);
    Serial.println();
  }

  stepper1.run();
  stepper2.run();

  if (largeStringInProgress && !largeStringAckReceived) {
    static unsigned long chunkSendInterval = 50;
    static unsigned long lastChunkSendTime = 0;

    if (millis() - lastChunkSendTime > chunkSendInterval && currentChunkIndex < totalChunks) {
      sendNextChunk();
      lastChunkSendTime = millis();
    }

    if (millis() - lastSendAttempt > resendDelay && currentChunkIndex >= totalChunks) {
      Serial.println("No confirmation received. Retrying large string transmission...");
      currentChunkIndex = 0;
      lastSendAttempt   = millis();
      sendNextChunk();
    }
  }

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

  handleSendTriggerCommand();
}

/************************************************************/
/*                     ISR                                  */
/************************************************************/
void IRAM_ATTR handleResetInterrupt() {
  Serial.println("Red button pressed, restarting esp32");
  esp_restart();
}
