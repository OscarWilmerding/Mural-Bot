#pragma once

#include <stdint.h>

// Hub MAC Address
extern uint8_t hubAddress[6];

// Solenoid pin configuration
constexpr uint8_t SOLENOID_PINS[] = {
    17, // 1
    21, // 2
    22, // 3
    25, // 4
    32, // 5
    15, // 6
    33, // 7
    27, // 8
    4,  // 9
    16, // 10
    26, // 11
    14, // 12
    13, // 13 (extra port)
    12  // 14 (buzzer)
};
constexpr int NUM_SOLENOIDS = sizeof(SOLENOID_PINS) / sizeof(SOLENOID_PINS[0]);

// Timing configuration
constexpr int MAX_LEDGER_SIZE = 100;
// Duration values are stored as milliseconds (allowing fractional ms)
extern float durationMs;              // serial "trig" pulse width (ms, may be fractional)
extern float preActivationDelay;      // used by serial commands (ms, may be fractional)
constexpr int fixedPostActivationDelay = 1000; // ms (fixed post-activation delay)
