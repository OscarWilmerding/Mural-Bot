#pragma once
#include <Arduino.h>
#include <esp_now.h> // Ensure this is included for esp_now_send_status_t
#include "state.h"

/*** gcode_loader.cpp ***/
void loadCommandsFromFile(const char *path);


/*** movement.cpp ***/
void move_to_position(float position1, float position2);
void move_to_position_blocking(float position1, float position2);
void calculateEndingLengths(const Command& cmd, float& endA, float& endB);
void four_corners();
void printCurrentPositions();
void determineStripeVelocities(float posA, float posB, float &velA, float &velB);

/*** comms.cpp ***/
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);
void onDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len);
void startLargeStringSend(const String &strToSend);
void sendTriggerCommand();
void handleSendTriggerCommand();
void sendSinglePaintBurst();
void startLargeStringSend(const String &strToSend);
void sendNextChunk();
void sendTriggerCommand();
void handleSendTriggerCommand();

/*** parser.cpp ***/
void processSerialCommand(String command);
void listAvailableCommands();
void startNextCommand();
