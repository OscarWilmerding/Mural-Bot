#pragma once

#include <Arduino.h>
#include <stdint.h>

// Hub MAC Address
extern uint8_t hubAddress[6];

// Solenoid pin configuration
// NOTE: GPIO 26 and 14 have been reassigned to heater pins
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
    13, // 13 (extra port)
    26,
//    14,
    12  // 14 (buzzer)
};
constexpr int NUM_SOLENOIDS = sizeof(SOLENOID_PINS) / sizeof(SOLENOID_PINS[0]);

// Heater pin configuration (PWM control)
constexpr uint8_t HEATER_1_PIN = 13;
constexpr uint8_t HEATER_2_PIN = 14;
constexpr int HEATER_PWM_FREQUENCY = 1000;  // 1 kHz standard PWM frequency
constexpr int HEATER_PWM_RESOLUTION = 8;    // 0-255 resolution

// Timing configuration
constexpr int MAX_LEDGER_SIZE = 100;
// Duration values stored in milliseconds (may be fractional)
extern float durationMs;              // serial "trig" pulse width (ms)
extern float preActivationDelay;      // used by serial commands (ms)
constexpr int fixedPostActivationDelay = 1000; // ms

// Hardware control functions
void pullSolenoid(int solenoidNumber, int level);
// Pulse a solenoid for a specified number of microseconds
void pullSolenoidForUs(int solenoidNumber, unsigned long microseconds);
void setAllPins(bool state);
void runCalibration(int solenoid, float lowMs, float highMs, float stepMs);
void initializeHeaterPWM();
void setHeaterPWM(uint8_t heaterNum, uint8_t dutyCyclePercent);

// Serial interface functions
void handleSerialCommands();
void printHelp();
