#pragma once

#include <Arduino.h>
#include <stdint.h>

// Hub MAC Address
extern uint8_t hubAddress[6];

// Button pin configuration
constexpr uint8_t GREEN_BUTTON_PIN  = 25;
constexpr uint8_t BLUE_BUTTON_PIN   = 26;
constexpr uint8_t YELLOW_BUTTON_PIN = 4;

// ─── Shift Register Configuration ────────────────────────────────────────────
//
// Three 16-bit blocks are daisy-chained for a 48-bit total chain.
// Each block drives one color/index of solenoids.
// Within each 16-bit block, solenoids occupy bits 3–12 (0-indexed).
// Bits 0–2 and 13–15 within each block are unused.
//
//   Block 1 (color 1, solenoids  1–10):  chain bits  3–12  (1-indexed:  4–13)
//   Block 2 (color 2, solenoids 11–20):  chain bits 19–28  (1-indexed: 20–29)
//   Block 3 (color 3, solenoids 21–30):  chain bits 35–44  (1-indexed: 36–45)
//
// ─────────────────────────────────────────────────────────────────────────────

// Shift register pins
constexpr uint8_t SR_DATA_PIN  = 23;   // SER   – serial data in
constexpr uint8_t SR_CLOCK_PIN = 18;   // SRCLK – shift clock
constexpr uint8_t SR_LATCH_PIN = 19;   // RCLK  – latch (transfers shift register to output)

// Chain geometry
constexpr int SR_TOTAL_BITS          = 48;   // total bits in the daisy-chain
constexpr int SR_BLOCK_SIZE          = 16;   // bits per block
constexpr int SR_BLOCK_OFFSET        =  3;   // 0-indexed offset to first solenoid within each block
constexpr int SR_SOLENOIDS_PER_BLOCK = 10;   // active solenoid bits per block (bits 3–12)
constexpr int NUM_SOLENOIDS          = 30;   // SR_SOLENOIDS_PER_BLOCK × 3 blocks

// Solenoid-to-bit lookup table
// SOLENOID_BIT[n-1] gives the 0-indexed bit position in the 48-bit chain for solenoid n.
constexpr uint8_t SOLENOID_BIT[NUM_SOLENOIDS] = {
    // Block 1 — color 1 — solenoids 1–10
     3,  4,  5,  6,  7,  8,  9, 10, 11, 12,
    // Block 2 — color 2 — solenoids 11–20
    19, 20, 21, 22, 23, 24, 25, 26, 27, 28,
    // Block 3 — color 3 — solenoids 21–30
    35, 36, 37, 38, 39, 40, 41, 42, 43, 44
};

// Physical spacing between blocks, measured from block 3 (the leading/reference block).
// Used to compute per-block trigger delays: delay_ms = spacing / stripe_velocity * 1000
constexpr float BLOCK2_SPACING_M = 0.044f;  // 44 mm  — block 3 to block 2
constexpr float BLOCK1_SPACING_M = 0.088f;  // 88 mm  — block 3 to block 1

// ─────────────────────────────────────────────────────────────────────────────

// Timing configuration
constexpr int MAX_LEDGER_SIZE = 100;
extern float durationMs;
extern float solenoidDurationMs[];
extern float preActivationDelay;
constexpr int fixedPostActivationDelay = 1000; // ms

// Helper function to get actual duration for a solenoid
float getActualDuration(int solenoidNumber);

// Hardware control functions
void initShiftRegisters();
void pullSolenoid(int solenoidNumber, int level);
void pullSolenoidForUs(int solenoidNumber, unsigned long microseconds);
void setSolenoidBit(int solenoidNumber, bool state); // set bit only, no shift-out
void commitShiftRegister();                          // shift out accumulated state
void setAllPins(bool state);

// Serial interface functions
void handleSerialCommands();
void printHelp();
