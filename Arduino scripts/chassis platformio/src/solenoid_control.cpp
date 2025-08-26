#include "hardware.h"

void pullSolenoid(int solenoidNumber, int level) {
    if (solenoidNumber < 1 || solenoidNumber > NUM_SOLENOIDS) return;
    digitalWrite(SOLENOID_PINS[solenoidNumber - 1], level);
}

void setAllPins(bool state) {
    for (int i = 0; i < NUM_SOLENOIDS; i++) {
        digitalWrite(SOLENOID_PINS[i], state ? HIGH : LOW);
    }
}

void runCalibration(int solenoid, int lowMs, int highMs, int stepMs) {
    if (solenoid < 1 || solenoid > NUM_SOLENOIDS) return;
    if (stepMs <= 0) stepMs = 1;

    int stepCount = 0;
    for (int width = lowMs; width <= highMs; width += stepMs) {
        ++stepCount;
        Serial.printf("Cal step %d: solenoid %d width %d ms\n", stepCount, solenoid, width);
        pullSolenoid(solenoid, HIGH);
        delay(width);
        pullSolenoid(solenoid, LOW);
        delay(1000);
    }
    pullSolenoid(solenoid, LOW);
    Serial.println("Calibration complete.");
}
