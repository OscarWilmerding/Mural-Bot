#pragma once
#include <Arduino.h>
#include <AccelStepper.h>

/************ data structures ************/
struct Position { float x; float y; };

struct Command {
  enum Type { MOVE, COLOR_CHANGE, STRIPE } type;
  Position pos;        // for MOVE
  String   colorHex;   // for COLOR_CHANGE
  String   stripeName; // for STRIPE
  float    drop;
  float    startPulleyA;
  float    startPulleyB;
  String   pattern;
};

constexpr int MAX_COMMANDS = 200;

/************ globals provided by main.cpp ************/
extern const int redButtonPin;
extern const int greyButtonPin;

extern AccelStepper stepper1;
extern AccelStepper stepper2;

extern bool sendTriggerAfterMovement;
extern bool runMode;
extern bool movementInProgress;
extern bool motor1Done;
extern bool motor2Done;

extern float baseAcceleration;
extern float baseMaxSpeed;
extern float accelerationMultiplier;
extern float maxSpeedMultiplier;

extern uint8_t chassisAddress[6];
extern bool commandConfirmed;
extern const uint8_t COMMAND_RUN;

typedef struct struct_message {
  uint8_t command;
  uint8_t chunkIndex;
} struct_message;

extern struct_message outgoingMessage;
extern struct_message incomingMessage;

extern bool          finalAckReceived;
extern bool          waitingForConfirmation;
extern unsigned long confirmationStartTime;
extern unsigned long confirmationTimeout;

extern float stepsPerMeter;
extern int   motor1Direction;
extern int   motor2Direction;
extern float motor1Position;
extern float motor2Position;

extern unsigned long prePokePause;
extern unsigned long chassisWaitTime;

extern float pulleySpacing;
extern int   numberOfDrawnColumns;
extern float stripeVelocity;

extern unsigned long velocityCalcDelay;

extern const int CHUNK_PAYLOAD_SIZE;
extern bool      largeStringInProgress;
extern String    largeStringToSend;
extern int       totalChunks;
extern int       currentChunkIndex;
extern bool      largeStringAckReceived;
extern unsigned long lastSendAttempt;
extern unsigned long resendDelay;

extern volatile bool startAckReceived;

extern Command commands[MAX_COMMANDS];
extern int     commandCount;
extern int     currentCommandIndex;

/************ isr from main.cpp ************/
void IRAM_ATTR handleResetInterrupt();
