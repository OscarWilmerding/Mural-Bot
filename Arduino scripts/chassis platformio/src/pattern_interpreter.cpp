#include "core.h"

ledgerEntry ledger[MAX_LEDGER_SIZE];

void initLedger() {
    for (int i = 0; i < MAX_LEDGER_SIZE; i++) {
        ledger[i] = { -1, 0, 0, false };
    }
}

void schedulePin(int solenoid, uint32_t delayFromNowMs, uint16_t widthMs) {
    if (solenoid < 1 || solenoid > NUM_SOLENOIDS) return;
    const uint32_t t = millis() + delayFromNowMs;
    // widthMs == 0 means use the global durationMs (may be fractional); round to nearest ms
    uint16_t actualWidth = widthMs == 0 ? (uint16_t)(durationMs + 0.5f) : widthMs;

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
    if (patternToProcess.length() < 4 || speed <= 0.0f) return;
    auto ms = [](float seconds) -> uint32_t { return (uint32_t)(seconds * 1000.0f + 0.5f); };

    for (int i = 0; i < 4; i++) {
        char c = patternToProcess[i];
        if (c == 'x') continue;


                //below, the leftmost nozzle triggers on a delay because its higher than next one
        if (c == '1') {
            if (i == 0) { schedulePin(1, ms(0.1f / speed)); }  //this equation for the time delay is correct trust
            if (i == 1) { schedulePin(2, 1); }
            if (i == 2) { schedulePin(3, ms(0.1f / speed)); }
            if (i == 3) { schedulePin(4, 1); }
            if (i == 4) { schedulePin(5, ms(0.1f / speed)); }
            if (i == 5) { schedulePin(6, 1); }
        } else if (c == '2') {
            Serial.println("C==2 this probably shouldn't happen");
        } else if (c == '3') {
            Serial.println("C==3 this probably shouldn't happen");
        }
    }
}

void sprayAndStripe(float stripeVelocity, float drop, String* patternList, int patternCount) {
    initLedger();

    if (stripeVelocity <= 0.0f || patternCount <= 0) return;

    const double movementTimeMs = (double)drop / (double)stripeVelocity * 1000.0;
    const double intervalMs = movementTimeMs / (double)patternCount;

    const uint32_t startMs = millis();
    double nextTriggerAt = (double)startMs + intervalMs;
    int triggerCount = 0;

    Serial.print("Total Movement Time: "); Serial.print(movementTimeMs); Serial.println(" ms");
    Serial.print("Interval between sprays: "); Serial.print(intervalMs); Serial.println(" ms");
    Serial.println("-- STARTING SPRAY STRIPE SOLENOID MOVEMENTS --");

    while ((double)(millis() - startMs) < movementTimeMs) {
        const uint32_t now = millis();

        if (triggerCount < patternCount && (double)now >= nextTriggerAt) {
            interpretPattern(patternList[triggerCount++], stripeVelocity);
            nextTriggerAt += intervalMs;
        }

        for (int i = 0; i < MAX_LEDGER_SIZE; i++) {
            if (ledger[i].solenoid == -1) continue;

            if (!ledger[i].triggered && now >= ledger[i].triggerTimeMs) {
                pullSolenoid(ledger[i].solenoid, HIGH);
                ledger[i].triggered = true;
            } else if (ledger[i].triggered && now >= ledger[i].triggerTimeMs + ledger[i].pulseWidthMs) {
                pullSolenoid(ledger[i].solenoid, LOW);
                ledger[i].solenoid = -1; // free the slot
            }
        }
    }

    Serial.println("Movement complete, turning off spray solenoids.");
    for (int s = 1; s <= 12; s++) pullSolenoid(s, LOW);
}
