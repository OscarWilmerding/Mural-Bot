#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "hardware.h"

// Ledger system
struct ledgerEntry {
    int      solenoid;        // 1..NUM_SOLENOIDS; -1 means empty
    uint32_t triggerTimeMs;   // absolute millis when to go HIGH
    uint16_t pulseWidthMs;    // width for this event
    bool     triggered;       // whether we've already gone HIGH
};

extern ledgerEntry ledger[MAX_LEDGER_SIZE];

// Pattern interpreter functions
void initLedger();
void schedulePin(int solenoid, uint32_t delayFromNowMs, uint16_t widthMs = 0);
void interpretPattern(const String& patternToProcess, float speed);
void sprayAndStripe(float stripeVelocity, float drop, String* patternList, int patternCount);

// ESP-NOW message struct
typedef struct struct_message {
    uint8_t command;
    uint8_t chunkIndex;
} struct_message;

// Communication functions
void initializeESPNow();
void handleLargeStringPacket(const uint8_t *data, int len);
void processReceivedString();
void onDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len);
void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status);

extern bool newMessageReady;
