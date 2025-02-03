#include <esp_now.h>
#include <WiFi.h>

// Define Chassis MAC address here
uint8_t chassisAddress[] = {0x48, 0x27, 0xE2, 0xE6, 0xE8, 0x34};

// Status flag for command completion
bool commandConfirmed = false;

// Define commands
const uint8_t COMMAND_RUN = 0x01;  // Example command to send

// Structure for sending and receiving data
typedef struct struct_message {
  uint8_t command;
} struct_message;

// Create message to send
struct_message outgoingMessage;

// Callback when data is sent
void onDataSent(const uint8_t *macAddr, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
  delay(50);  // Small delay to ensure Serial prints completely
}

// Callback when data is received
void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  Serial.println("Confirmation received from Chassis");
  commandConfirmed = true;  // Set the flag when confirmation is received
}

void setup() {
  Serial.begin(115200);

  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  Serial.println("ESP-NOW Hub Initialized");

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register sending and receiving callbacks
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  // Register peer
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, chassisAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }
}

// Function to send the trigger command
void sendTriggerCommand() {
  commandConfirmed = false;  // Reset confirmation flag

  // Prepare and send command to Chassis
  outgoingMessage.command = COMMAND_RUN;
  Serial.println("Sending command to Chassis...");
  
  esp_err_t result = esp_now_send(chassisAddress, (uint8_t *) &outgoingMessage, sizeof(outgoingMessage));

  if (result != ESP_OK) {
    Serial.println("Error: Failed to send command. Re-adding peer.");
    esp_now_del_peer(chassisAddress);
    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, chassisAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
  }

  // Wait for confirmation with a timeout of 3 seconds
  unsigned long startTime = millis();
  while (!commandConfirmed && millis() - startTime < 3000) {  // 3-second timeout
    delay(100);  // Short delay while waiting for response
  }

  if (commandConfirmed) {
    Serial.println("Command confirmed by Chassis");
  } else {
    Serial.println("No response from Chassis. Moving on...");
  }
}

void loop() {
  // Send the trigger command every 10 seconds
  sendTriggerCommand();
  delay(10000);  // Wait 10 seconds before sending the next command
}
