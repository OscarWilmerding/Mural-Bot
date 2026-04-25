#include "core.h"

ledgerEntry ledger[MAX_LEDGER_SIZE];

static int      dbg_scheduled   = 0;
static int      dbg_coalesced   = 0;
static int      dbg_patterns    = 0;
static int      dbg_ledger_full = 0;
static int      dbg_orphans     = 0;
static uint32_t dbg_max_late_ms = 0;
static uint32_t dbg_max_gap_ms  = 0;
static int      dbg_fires[NUM_SOLENOIDS];

void initLedger() {
    for (int i = 0; i < MAX_LEDGER_SIZE; i++) {
        ledger[i] = { -1, 0, 0, false };
    }
}

void schedulePin(int solenoid, uint32_t delayFromNowMs, uint16_t widthMs) {
    if (solenoid < 1 || solenoid > NUM_SOLENOIDS) return;
    const uint32_t t = millis() + delayFromNowMs;
    uint16_t actualWidth = widthMs == 0 ? (uint16_t)(getActualDuration(solenoid) + 0.5f) : widthMs;

    // Simple coalescing
    for (int i = 0; i < MAX_LEDGER_SIZE; i++) {
        if (ledger[i].solenoid == solenoid && !ledger[i].triggered) {
            uint32_t a = ledger[i].triggerTimeMs, b = t;
            uint32_t diff = (a > b) ? (a - b) : (b - a);
            if (diff < actualWidth) {
                dbg_coalesced++;
                Serial.printf("COALESCED sol%d diff=%lums width=%ums\n", solenoid, (unsigned long)diff, (unsigned)actualWidth);
                return;
            }
        }
    }

    // Insert
    for (int i = 0; i < MAX_LEDGER_SIZE; i++) {
        if (ledger[i].solenoid == -1) {
            ledger[i] = { solenoid, t, actualWidth, false };
            dbg_scheduled++;
            return;
        }
    }

    dbg_ledger_full++;
    Serial.printf("WARNING: Ledger full. Could not schedule solenoid %d at %lu ms\n",
                solenoid, (unsigned long)t);
}

void interpretPattern(const String& patternToProcess, float speed) {
    if (patternToProcess.length() < 4 || speed <= 0.0f) return;

    for (int i = 0; i < 8; i++) {
        char c = patternToProcess[i];
        if (c == 'x') continue;

        if (c == '1') {
            schedulePin(i + 1, 0);
        } else if (c == '2') {
            // color index 2 — not active in current layer, no action
        } else if (c == '3') {
            // color index 3 — not active in current layer, no action
        }
    }
}

void sprayAndStripe(float stripeVelocity, float drop, String* patternList, int patternCount) {
    dbg_scheduled   = 0;
    dbg_coalesced   = 0;
    dbg_patterns    = 0;
    dbg_ledger_full = 0;
    dbg_orphans     = 0;
    dbg_max_late_ms = 0;
    dbg_max_gap_ms  = 0;
    memset(dbg_fires, 0, sizeof(dbg_fires));
    initLedger();

    if (stripeVelocity <= 0.0f || patternCount <= 0) return;

    const double movementTimeMs = (double)drop / (double)stripeVelocity * 1000.0;
    const double intervalMs     = movementTimeMs / (double)patternCount;

    const uint32_t startMs    = stripeReceiveTime;
    double nextTriggerAt      = (double)startMs + intervalMs;
    int    triggerCount       = 0;

    Serial.print("Total Movement Time: "); Serial.print(movementTimeMs); Serial.println(" ms");
    Serial.print("Interval between sprays: "); Serial.print(intervalMs); Serial.println(" ms");
    Serial.println("-- STARTING SPRAY STRIPE SOLENOID MOVEMENTS --");

    stopRequested = false;
    uint32_t prevLoopMs = 0;

    while (!stopRequested && (double)(millis() - startMs) < movementTimeMs) {
        const uint32_t now = millis();

        // Detect loop stalls — FreeRTOS preemption, Serial blocking, etc.
        if (prevLoopMs) {
            uint32_t gap = now - prevLoopMs;
            if (gap > dbg_max_gap_ms) dbg_max_gap_ms = gap;
        }
        prevLoopMs = now;

        if (triggerCount < patternCount && (double)now >= nextTriggerAt) {
            // How many ms past the ideal fire window did this pattern execute?
            double lateMs_f = (double)now - nextTriggerAt;
            uint32_t lateMs = (lateMs_f > 0.0) ? (uint32_t)(lateMs_f + 0.5) : 0;
            if (lateMs > dbg_max_late_ms) dbg_max_late_ms = lateMs;

            dbg_patterns++;
            interpretPattern(patternList[triggerCount++], stripeVelocity);
            nextTriggerAt += intervalMs;
        }

        for (int i = 0; i < MAX_LEDGER_SIZE; i++) {
            if (ledger[i].solenoid == -1) continue;

            if (!ledger[i].triggered && now >= ledger[i].triggerTimeMs) {
                pullSolenoid(ledger[i].solenoid, HIGH);
                dbg_fires[ledger[i].solenoid - 1]++;
                ledger[i].triggered = true;
            } else if (ledger[i].triggered && now >= ledger[i].triggerTimeMs + ledger[i].pulseWidthMs) {
                pullSolenoid(ledger[i].solenoid, LOW);
                ledger[i].solenoid = -1;
            }
        }
    }

    // Ledger entries still open: either fired but not yet closed (HIGH stuck on), or never fired at all
    for (int i = 0; i < MAX_LEDGER_SIZE; i++) {
        if (ledger[i].solenoid != -1) dbg_orphans++;
    }

    if (stopRequested) {
        Serial.println("STOP received mid-stripe: aborting.");
        for (int s = 1; s <= NUM_SOLENOIDS; s++) pullSolenoid(s, LOW);
    } else {
        Serial.println("Movement complete, turning off spray solenoids.");
        for (int s = 1; s <= 12; s++) pullSolenoid(s, LOW);
    }

    // Per-solenoid fire summary — only active solenoids shown
    char firesStr[64] = "";
    int  pos        = 0;
    int  totalFires = 0;
    for (int s = 0; s < NUM_SOLENOIDS; s++) {
        totalFires += dbg_fires[s];
        if (dbg_fires[s] > 0)
            pos += snprintf(firesStr + pos, sizeof(firesStr) - pos, "s%d:%d ", s + 1, dbg_fires[s]);
    }
    if (pos == 0) strncpy(firesStr, "none", sizeof(firesStr));

    snprintf(diagLog, DIAG_LOG_SIZE,
             "patterns=%d/%d  sched=%d  fires=%d(%s)  coal=%d  full=%d  orphans=%d  maxLate=%lums  maxGap=%lums  pulse=%dms",
             dbg_patterns, patternCount,
             dbg_scheduled, totalFires, firesStr,
             dbg_coalesced, dbg_ledger_full, dbg_orphans,
             (unsigned long)dbg_max_late_ms, (unsigned long)dbg_max_gap_ms,
             (int)(getActualDuration(1) + 0.5f));
    Serial.print("DIAG: "); Serial.println(diagLog);
}
