#pragma once

#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>

// ESP-NOW message struct
typedef struct struct_message {
    uint8_t command;
    uint8_t chunkIndex;
} struct_message;

void initializeESPNow();
void handleLargeStringPacket(const uint8_t *data, int len);
void processReceivedString();
void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len);
void onDataSent(const esp_now_send_info_t *info, esp_now_send_status_t status);

extern bool newMessageReady;
