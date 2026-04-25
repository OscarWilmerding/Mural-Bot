#include "core.h"
#include "serial_commands.h"

ledgerEntry ledger[MAX_LEDGER_SIZE];

void initLedger() {
    for (int i = 0; i < MAX_LEDGER_SIZE; i++) {
        ledger[i] = { -1, 0, 0, false };
    }
}

void schedulePin(int solenoid, uint32_t delayFromNowMs, uint16_t widthMs) {
    if (solenoid < 1 || solenoid > NUM_SOLENOIDS) return;
    const uint32_t t = millis() + delayFromNowMs;
    // widthMs == 0 means use the per-solenoid duration (may be fractional); round to nearest ms
    uint16_t actualWidth = widthMs == 0 ? (uint16_t)(getActualDuration(solenoid) + 0.5f) : widthMs;

    // Simple coalescing
    for (int i = 0; i < MAX_LEDGER_SIZE; i++) {
        if (ledger[i].solenoid == solenoid && !ledger[i].triggered) {
            uint32_t a = ledger[i].triggerTimeMs, b = t;
            uint32_t diff = (a > b) ? (a - b) : (b - a);
            if (diff < actualWidth) return; // close enough, don't double-schedule
        }
    }

    // Insert
    for (int i = 0; i < MAX_LEDGER_SIZE; i++) {
        if (ledger[i].solenoid == -1) {
            ledger[i] = { solenoid, t, actualWidth, false };
            return;
        }
    }

    Serial.printf("WARNING: Ledger full. Could not schedule solenoid %d at %lu ms\n",
                solenoid, (unsigned long)t);
}

void interpretPattern(const String& patternToProcess, float speed) {
    if (patternToProcess.length() < 1 || speed <= 0.0f) return;

    const uint32_t block1DelayMs = (uint32_t)(BLOCK1_SPACING_M / speed * 1000.0f + 0.5f);
    const uint32_t block2DelayMs = (uint32_t)(BLOCK2_SPACING_M / speed * 1000.0f + 0.5f);

    for (int i = 0; i < SR_SOLENOIDS_PER_BLOCK; i++) {
        if (i >= (int)patternToProcess.length()) break;
        char c = patternToProcess[i];
        if (c == 'x' || c == 'X') continue;

        if      (c == '1') schedulePin(i + 1,  block1DelayMs);
        else if (c == '2') schedulePin(i + 11, block2DelayMs);
        else if (c == '3') schedulePin(i + 21, 0);
    }
}

void sprayAndStripe(float stripeVelocity, float drop, String* patternList, int patternCount) {
    initLedger();

    if (stripeVelocity <= 0.0f || patternCount <= 0) return;

    const double movementTimeMs = (double)drop / (double)stripeVelocity * 1000.0;
    const double intervalMs = movementTimeMs / (double)patternCount;

    const uint32_t startMs = stripeReceiveTime;
    double nextTriggerAt = (double)startMs + intervalMs;
    int triggerCount = 0;

    Serial.print("Total Movement Time: "); Serial.print(movementTimeMs); Serial.println(" ms");
    Serial.print("Interval between sprays: "); Serial.print(intervalMs); Serial.println(" ms");
    Serial.println("-- STARTING SPRAY STRIPE SOLENOID MOVEMENTS --");

    stopRequested = false;
    bool lastYellowStripe = digitalRead(YELLOW_BUTTON_PIN);
    while (!stopRequested && (double)(millis() - startMs) < movementTimeMs) {
        const uint32_t now = millis();

        bool yellowNow = digitalRead(YELLOW_BUTTON_PIN);
        if (yellowNow == LOW && lastYellowStripe == HIGH) processCommand("trig");
        lastYellowStripe = yellowNow;

        if (triggerCount < patternCount && (double)now >= nextTriggerAt) {
            interpretPattern(patternList[triggerCount++], stripeVelocity);
            nextTriggerAt += intervalMs;
        }

        bool anyChange = false;
        for (int i = 0; i < MAX_LEDGER_SIZE; i++) {
            if (ledger[i].solenoid == -1) continue;

            if (!ledger[i].triggered && now >= ledger[i].triggerTimeMs) {
                setSolenoidBit(ledger[i].solenoid, true);
                ledger[i].triggered = true;
                anyChange = true;
            } else if (ledger[i].triggered && now >= ledger[i].triggerTimeMs + ledger[i].pulseWidthMs) {
                setSolenoidBit(ledger[i].solenoid, false);
                ledger[i].solenoid = -1;
                anyChange = true;
            }
        }
        if (anyChange) commitShiftRegister();
    }

    if (stopRequested) {
        Serial.println("STOP received mid-stripe: aborting.");
        for (int s = 1; s <= NUM_SOLENOIDS; s++) pullSolenoid(s, LOW);
        return;
    }

    Serial.println("Movement complete, turning off spray solenoids.");
    for (int s = 1; s <= NUM_SOLENOIDS; s++) pullSolenoid(s, LOW);
}
