#include "core.h"

// Communication state
static bool   largeStringActive   = false;
static int    expectedChunks      = 0;
static int    receivedChunks      = 0;
static String largeStringBuffer;
bool newMessageReady = false;

struct_message incomingMessage;
struct_message confirmationMessage;

void initializeESPNow() {
    WiFi.mode(WIFI_STA);

    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    Serial.print("MAC address: ");
    Serial.println(WiFi.macAddress());
    Serial.println("ESP-NOW Chassis Initialized");

    esp_now_register_send_cb(onDataSent);
    esp_now_register_recv_cb(onDataRecv);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, hubAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    }
}

void handleLargeStringPacket(const uint8_t *data, int len) {
    if (len < 1) return;
    uint8_t packetType = data[0];

    if (packetType == 0x10) {
        expectedChunks = (data[1] << 8) | data[2];
        receivedChunks = 0;
        largeStringBuffer.reserve(expectedChunks * 200 + 1);
        largeStringBuffer = "";
        largeStringActive = true;

        // ACK start
        struct_message startAck;
        startAck.command    = 0x10;
        startAck.chunkIndex = 0;
        esp_now_send(hubAddress, (uint8_t *)&startAck, sizeof(startAck));

        Serial.print("Large string incoming; total chunks: ");
        Serial.println(expectedChunks);
        return;
    }

    if (packetType == 0x11 && largeStringActive) {
        const uint8_t chunkIndex = data[1];

        int payloadLen = len - 2;
        if (payloadLen > 0 && data[2 + payloadLen - 1] == '\0') {
            payloadLen--;
        }
        largeStringBuffer.concat((const char*)&data[2], payloadLen);

        receivedChunks++;

        struct_message chunkAck{0x13, chunkIndex};
        esp_now_send(hubAddress, (uint8_t *)&chunkAck, sizeof(chunkAck));

        if (receivedChunks >= expectedChunks) {
            struct_message confirmMsg{0x12, 0};
            esp_now_send(hubAddress, (uint8_t *)&confirmMsg, sizeof(confirmMsg));

            largeStringActive = false;
            newMessageReady = true;
        }
    }
}

void processReceivedString() {
    Serial.println("Processing received large string...");
    Serial.println("Full received data:");
    Serial.println(largeStringBuffer);

    // Check for special 'paintburst' command
    if (largeStringBuffer == "paint burst") {
        Serial.println("Triggering solenoids");
        setAllPins(true);
        delay(durationMs);
        setAllPins(false);
        delay(fixedPostActivationDelay);
        largeStringBuffer = "";
        return;
    }

    DynamicJsonDocument doc(16384);
    DeserializationError error = deserializeJson(doc, largeStringBuffer);

    // free buffer early
    String tmp = largeStringBuffer;
    largeStringBuffer = "";

    if (error) {
        Serial.print("JSON Parsing Failed: ");
        Serial.println(error.c_str());
        return;
    }

    const char* stripeName     = doc["stripeName"] | "";
    float drop                 = doc["drop"] | 0.0f;
    float startPulleyA         = doc["startPulleyA"] | 0.0f;
    float startPulleyB         = doc["startPulleyB"] | 0.0f;
    float stripeVelocity       = doc["stripeVelocity"] | 0.0f;
    JsonArray patternArray     = doc["pattern"].as<JsonArray>();
    int patternCount           = patternArray.size();

    String* patternList = new String[patternCount];
    int i = 0;
    for (JsonVariant v : patternArray) {
        if (i < patternCount) patternList[i++] = v.as<String>();
        else break;
    }

    Serial.println("Extracted Data:");
    Serial.print("Stripe Name: ");      Serial.println(stripeName);
    Serial.print("Drop: ");             Serial.println(drop, 4);
    Serial.print("Start Pulley A: ");   Serial.println(startPulleyA, 4);
    Serial.print("Start Pulley B: ");   Serial.println(startPulleyB, 4);
    Serial.print("Stripe Velocity: ");  Serial.println(stripeVelocity, 4);
    Serial.print("Pattern Count: ");    Serial.println(patternCount);

    sprayAndStripe(stripeVelocity, drop, patternList, patternCount);
    delete[] patternList;
}

void onDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
    if (len >= 1) handleLargeStringPacket(incomingData, len);
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("Send Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}
