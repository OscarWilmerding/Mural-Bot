#include <Arduino.h>
#include "hardware.h"
#include "core.h"

// Global variables from config.h
uint8_t hubAddress[] = {0x48, 0x27, 0xE2, 0xE6, 0xE6, 0x58};
char diagLog[DIAG_LOG_SIZE] = "no stripe run yet";
float durationMs = 10.0f; // ms (may be fractional) - global default
float solenoidDurationMs[14]; // per-solenoid overrides (will be initialized to durationMs in setup)
float preActivationDelay = 0.0f; // ms (may be fractional)

void setup() {
    Serial.begin(115200);
    delay(2000);

    // Initialize all solenoid pins
    for (int i = 0; i < NUM_SOLENOIDS; i++) {
        pinMode(SOLENOID_PINS[i], OUTPUT);
        digitalWrite(SOLENOID_PINS[i], LOW);
        solenoidDurationMs[i] = durationMs;  // Initialize per-solenoid durations to global default
    }

    initializeHeaterPWM();
    initLedger();
    initializeESPNow();

    // ADC2 channels are unavailable once Wi‑Fi is enabled, which is what
    // triggered the startup error on GPIO0.
    //
    // Instead of sampling an analog pin after ESP‑NOW starts, seed the RNG
    // from the ESP32's hardware random number generator.  Alternatively you
    // could move the analogRead() above initializeESPNow() or use an ADC1
    // pin, but esp_random() is easiest and avoids any ADC2/Wi‑Fi conflict.
    randomSeed(esp_random());
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
