#include <Arduino.h>
#include "hardware.h"
#include "core.h"

// Global variables from config.h
uint8_t hubAddress[] = {0x48, 0x27, 0xE2, 0xE6, 0xE6, 0x58};
int durationMs = 100;
int preActivationDelay = 0;

void setup() {
    Serial.begin(115200);
    delay(2000);

    // Initialize all solenoid pins
    for (int i = 0; i < NUM_SOLENOIDS; i++) {
        pinMode(SOLENOID_PINS[i], OUTPUT);
        digitalWrite(SOLENOID_PINS[i], LOW);
    }

    initLedger();
    initializeESPNow();

    randomSeed(analogRead(0));
    Serial.println("GPIO Control Script Initialized.");
}

void loop() {
    // Handle deferred message parsing
    if (newMessageReady) {
        newMessageReady = false;
        processReceivedString();
    }

    // Handle serial commands
    handleSerialCommands();
}
