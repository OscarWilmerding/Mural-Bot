#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "state.h"
#include "modules.h"

static void sendStartPacket() {
  uint8_t startPacket[3] = {
    0x10,
    (uint8_t)((totalChunks >> 8) & 0xFF),
    (uint8_t)(totalChunks & 0xFF)
  };
  esp_err_t result = esp_now_send(chassisAddress, startPacket, sizeof(startPacket));
  if (result != ESP_OK) {
    Serial.println("Failed to send start packet");
  }
}

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void onDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
  if (len == (int)sizeof(struct_message)) {
    memcpy(&incomingMessage, incomingData, sizeof(incomingMessage));
    if (incomingMessage.command == 0x10) {      // start-ack
      startAckReceived = true;
    } else if (incomingMessage.command == 0x13) {
      if (incomingMessage.chunkIndex == currentChunkIndex) {
        if (!largeStringAckReceived) {
          largeStringAckReceived = true;
          currentChunkIndex++;
        }
      }
    } else if (incomingMessage.command == 0x12) {
      finalAckReceived = true;
    }
  }
}

void startLargeStringSend(const String &strToSend) {
  largeStringToSend      = strToSend;
  int len                = largeStringToSend.length();
  totalChunks            = (len + CHUNK_PAYLOAD_SIZE - 1) / CHUNK_PAYLOAD_SIZE;
  currentChunkIndex      = 0;
  largeStringInProgress  = true;
  largeStringAckReceived = false;
  finalAckReceived       = false;
  startAckReceived       = false;

  Serial.println("Initiating large string send...");
  sendStartPacket();

  unsigned long t0 = millis();
  const unsigned long startTimeout = 1000;
  while (!startAckReceived) {
    if (millis() - t0 > startTimeout) {
      Serial.println("No start-ack, resending 0x10...");
      sendStartPacket();
      t0 = millis();
    }
    delay(10);
  }
  Serial.println("Start-ack received.");

  for (currentChunkIndex = 0; currentChunkIndex < totalChunks; ) {
    largeStringAckReceived = false;
    sendNextChunk();

    unsigned long sendTime = millis();
    const unsigned long timeout = 500;

    while (!largeStringAckReceived) {
      if (millis() - sendTime > timeout) {
        if (currentChunkIndex == 0) sendStartPacket();
        Serial.println("Chunk-ack timeout, resending chunk...");
        sendNextChunk();
        sendTime = millis();
      }
      delay(10);
    }
  }

  unsigned long finalT = millis();
  while (!finalAckReceived && millis() - finalT < 3000) delay(10);

  largeStringInProgress = false;
  Serial.println(finalAckReceived ? "Entire large string confirmed." : "Timed out waiting for final ack.");
}

void sendNextChunk() {
  if (!largeStringInProgress) return;
  if (currentChunkIndex >= totalChunks) {
    Serial.println("All chunks sent. Waiting for confirmation...");
    return;
  }

  int startIdx = currentChunkIndex * CHUNK_PAYLOAD_SIZE;
  String chunkData = largeStringToSend.substring(startIdx, startIdx + CHUNK_PAYLOAD_SIZE);

  size_t packetSize = 2 + chunkData.length();
  uint8_t *packet   = (uint8_t *) malloc(packetSize);
  packet[0] = 0x11;
  packet[1] = (uint8_t) currentChunkIndex;
  memcpy(&packet[2], chunkData.c_str(), chunkData.length());

  esp_err_t result = esp_now_send(chassisAddress, packet, packetSize);
  free(packet);

  if (result == ESP_OK) {
    Serial.print("Sent chunk #");
    Serial.print(currentChunkIndex);
    Serial.print(" of ");
    Serial.println(totalChunks);
  } else {
    Serial.print("Failed to send chunk #");
    Serial.println(currentChunkIndex);
  }
}

void sendTriggerCommand() {
  if (!waitingForConfirmation) {
    commandConfirmed = false;
    waitingForConfirmation = true;
    confirmationStartTime = millis();

    outgoingMessage.command = COMMAND_RUN;
    Serial.println("Sending command to Chassis...");

    esp_err_t result = esp_now_send(chassisAddress, (uint8_t *)&outgoingMessage, sizeof(outgoingMessage));
    if (result != ESP_OK) {
      Serial.println("Error: Failed to send command. Re-adding peer.");
      esp_now_del_peer(chassisAddress);

      esp_now_peer_info_t peerInfo = {};
      memcpy(peerInfo.peer_addr, chassisAddress, 6);
      peerInfo.channel = 0;
      peerInfo.encrypt = false;
      esp_now_add_peer(&peerInfo);
    }
  }
}

void handleSendTriggerCommand() {
  if (waitingForConfirmation) {
    if (commandConfirmed) {
      Serial.println("Command confirmed by Chassis");
      waitingForConfirmation = false;
      commandConfirmed        = false;
      if (runMode) {
        startNextCommand();
      }
    }
    else if (millis() - confirmationStartTime >= chassisWaitTime) {
      Serial.println("Chassis wait time exceeded. Moving on...");
      waitingForConfirmation = false;
      if (runMode) {
        startNextCommand();
      }
    }
  }
}
