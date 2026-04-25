#include "hardware.h"

// 48-bit state buffer representing the current output of the shift register chain.
// shiftState[0] = register 1 (chain bits 0–7), shiftState[5] = register 6 (chain bits 40–47).
static uint8_t shiftState[SR_TOTAL_BITS / 8] = {};

static void updateShiftRegister() {
    digitalWrite(SR_LATCH_PIN, LOW);
    // Shift bytes from last to first: each earlier byte pushes later bytes toward the far end.
    for (int i = (SR_TOTAL_BITS / 8) - 1; i >= 0; i--) {
        shiftOut(SR_DATA_PIN, SR_CLOCK_PIN, MSBFIRST, shiftState[i]);
    }
    digitalWrite(SR_LATCH_PIN, HIGH);
}

static void setBit(uint8_t bitPos, bool state) {
    uint8_t mask = 1 << (bitPos % 8);
    if (state) shiftState[bitPos / 8] |=  mask;
    else        shiftState[bitPos / 8] &= ~mask;
}

void initShiftRegisters() {
    pinMode(SR_DATA_PIN,  OUTPUT);
    pinMode(SR_CLOCK_PIN, OUTPUT);
    pinMode(SR_LATCH_PIN, OUTPUT);
    digitalWrite(SR_DATA_PIN,  LOW);
    digitalWrite(SR_CLOCK_PIN, LOW);
    memset(shiftState, 0, sizeof(shiftState));
    updateShiftRegister();
}

void pullSolenoid(int solenoidNumber, int level) {
    if (solenoidNumber < 1 || solenoidNumber > NUM_SOLENOIDS) return;
    setBit(SOLENOID_BIT[solenoidNumber - 1], level == HIGH);
    updateShiftRegister();
}

void pullSolenoidForUs(int solenoidNumber, unsigned long microseconds) {
    if (solenoidNumber < 1 || solenoidNumber > NUM_SOLENOIDS) return;
    setBit(SOLENOID_BIT[solenoidNumber - 1], true);
    updateShiftRegister();
    if (microseconds > 0) delayMicroseconds(microseconds);
    setBit(SOLENOID_BIT[solenoidNumber - 1], false);
    updateShiftRegister();
}

float getActualDuration(int solenoidNumber) {
    if (solenoidNumber < 1 || solenoidNumber > NUM_SOLENOIDS) return durationMs;
    return solenoidDurationMs[solenoidNumber - 1];
}

void setSolenoidBit(int solenoidNumber, bool state) {
    if (solenoidNumber < 1 || solenoidNumber > NUM_SOLENOIDS) return;
    setBit(SOLENOID_BIT[solenoidNumber - 1], state);
}

void commitShiftRegister() {
    updateShiftRegister();
}

void setAllPins(bool state) {
    if (!state) {
        memset(shiftState, 0, sizeof(shiftState));
    } else {
        for (int i = 0; i < NUM_SOLENOIDS; i++) {
            setBit(SOLENOID_BIT[i], true);
        }
    }
    updateShiftRegister();
}
