#pragma once

#include <Arduino.h>
#include "config.h"

struct ledgerEntry {
    int      solenoid;        // 1..NUM_SOLENOIDS; -1 means empty
    uint32_t triggerTimeMs;   // absolute millis when to go HIGH
    uint16_t pulseWidthMs;    // width for this event
    bool     triggered;       // whether we've already gone HIGH
};

extern ledgerEntry ledger[MAX_LEDGER_SIZE];

void initLedger();
void schedulePin(int solenoid, uint32_t delayFromNowMs, uint16_t widthMs = 0);
void interpretPattern(const String& patternToProcess, float speed);
void sprayAndStripe(float stripeVelocity, float drop, String* patternList, int patternCount);
