#include "hardware.h"

void pullSolenoid(int solenoidNumber, int level) {
    if (solenoidNumber < 1 || solenoidNumber > NUM_SOLENOIDS) return;
    digitalWrite(SOLENOID_PINS[solenoidNumber - 1], level);
}

// Pulse a solenoid for a specified number of microseconds
void pullSolenoidForUs(int solenoidNumber, unsigned long microseconds) {
    if (solenoidNumber < 1 || solenoidNumber > NUM_SOLENOIDS) return;
    digitalWrite(SOLENOID_PINS[solenoidNumber - 1], HIGH);
    if (microseconds > 0) delayMicroseconds(microseconds);
    digitalWrite(SOLENOID_PINS[solenoidNumber - 1], LOW);
}

void setAllPins(bool state) {
    for (int i = 0; i < NUM_SOLENOIDS; i++) {
        digitalWrite(SOLENOID_PINS[i], state ? HIGH : LOW);
    }
}

void runCalibration(int solenoid, float lowMs, float highMs, float stepMs) {
    if (solenoid < 1 || solenoid > NUM_SOLENOIDS) return;
    if (stepMs <= 0.0f) stepMs = 0.1f; // default small step

    int stepCount = 0;
    for (float width = lowMs; width <= highMs; width += stepMs) {
        ++stepCount;
        Serial.print("Cal step "); Serial.print(stepCount);
        Serial.print(": solenoid "); Serial.print(solenoid);
        Serial.print(" width "); Serial.print(width); Serial.println(" ms");
        unsigned long usec = (unsigned long)(width * 1000.0f + 0.5f);
        pullSolenoidForUs(solenoid, usec);
        delay(1000);
    }
    pullSolenoid(solenoid, LOW);
    Serial.println("Calibration complete.");
}
// Heater PWM control using ESP32 LEDC peripheral
void initializeHeaterPWM() {
    Serial.println("Initializing Heater PWM...");
    
    // Configure LEDC channel for heater 1
    Serial.printf("  Heater 1: GPIO %d\n", HEATER_1_PIN);
    ledcSetup(0, HEATER_PWM_FREQUENCY, HEATER_PWM_RESOLUTION);
    ledcAttachPin(HEATER_1_PIN, 0);
    Serial.println("  Heater 1 configured");
    
    // Configure LEDC channel for heater 2
    Serial.printf("  Heater 2: GPIO %d\n", HEATER_2_PIN);
    ledcSetup(1, HEATER_PWM_FREQUENCY, HEATER_PWM_RESOLUTION);
    ledcAttachPin(HEATER_2_PIN, 1);
    Serial.println("  Heater 2 configured");
    
    // Initialize both heaters to 0%
    ledcWrite(0, 0);
    ledcWrite(1, 0);
    
    Serial.println("Heater PWM initialized");
}

void setHeaterPWM(uint8_t heaterNum, uint8_t dutyCyclePercent) {
    // Validate input
    if (dutyCyclePercent > 100) {
        Serial.printf("ERROR: Duty cycle %d%% exceeds maximum 100%%\n", dutyCyclePercent);
        return;
    }
    
    // Convert percentage (0-100) to duty value (0-255)
    uint8_t dutyValue = (dutyCyclePercent * 255) / 100;
    
    if (heaterNum == 1) {
        Serial.printf("DEBUG: Setting Heater 1 to %d%% (duty=%d/255)\n", dutyCyclePercent, dutyValue);
        ledcWrite(0, dutyValue);
    }
    else if (heaterNum == 2) {
        Serial.printf("DEBUG: Setting Heater 2 to %d%% (duty=%d/255)\n", dutyCyclePercent, dutyValue);
        ledcWrite(1, dutyValue);
    }
    else {
        Serial.printf("ERROR: Invalid heater number %d (must be 1 or 2)\n", heaterNum);
    }
}