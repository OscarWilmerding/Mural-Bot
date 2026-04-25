#include <Arduino.h>
#include <esp_mac.h>
#include "hardware.h"
#include "core.h"
#include "serial_commands.h"

uint8_t hubAddress[] = {0x48, 0x27, 0xE2, 0xE6, 0xE6, 0x58};
static uint8_t chassisMAC[] = { 0xE8, 0x6B, 0xEA, 0xFC, 0xD4, 0xA4 };
float durationMs = 10.0f;
float solenoidDurationMs[30];
float preActivationDelay = 0.0f;

static bool lastGreen  = HIGH;
static bool lastBlue   = HIGH;
static bool lastYellow = HIGH;

// 0 = fluid control, 1 = movement control (-), 2 = movement control (+)
static int currentMode = 0;

void setup() {
    Serial.begin(115200);
    delay(2000);

    pinMode(GREEN_BUTTON_PIN,  INPUT_PULLUP);
    pinMode(BLUE_BUTTON_PIN,   INPUT_PULLUP);
    pinMode(YELLOW_BUTTON_PIN, INPUT_PULLUP);

    esp_base_mac_addr_set(chassisMAC);

    initShiftRegisters();
    initLedger();
    initializeESPNow();
    randomSeed(esp_random());
    Serial.println("GPIO Control Script Initialized.");
}

void loop() {
    if (newMessageReady) {
        newMessageReady = false;
        processReceivedString();
    }

    handleSerialCommands();

    bool g = digitalRead(GREEN_BUTTON_PIN);
    bool b = digitalRead(BLUE_BUTTON_PIN);
    bool y = digitalRead(YELLOW_BUTTON_PIN);

    if (g == LOW && lastGreen == HIGH) {
        currentMode = (currentMode + 1) % 3;
        switch (currentMode) {
            case 0: Serial.println("Mode: Fluid Control"); break;
            case 1: Serial.println("Mode: Move (-)");      break;
            case 2: Serial.println("Mode: Move (+)");      break;
        }
    }

    if (b == LOW && lastBlue == HIGH) {
        if (currentMode == 0) {
            processCommand("trig");
        } else {
            uint8_t cmd = (currentMode == 1) ? 0x30 : 0x32;
            esp_now_send(hubAddress, &cmd, 1);
        }
    }

    if (y == LOW && lastYellow == HIGH) {
        if (currentMode == 0) {
            float savedDuration = durationMs;
            processCommand("50");
            delay(500);
            for (int i = 0; i < 5; i++) {
                processCommand("trig");
                if (i < 4) delay(1000);
            }
            processCommand(String(savedDuration, 3));
        } else {
            uint8_t cmd = (currentMode == 1) ? 0x31 : 0x33;
            esp_now_send(hubAddress, &cmd, 1);
        }
    }

    lastGreen  = g;
    lastBlue   = b;
    lastYellow = y;
}
