#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>

// Define Hub MAC address
uint8_t hubAddress[] = {0x48, 0x27, 0xE2, 0xE6, 0xE6, 0x58};

// GPIO pin setup for serial commands
const int pins[] = {17, 21, 22, 25, 32, 15, 33, 27, 4, 16, 26, 14, 13, 12};
const int numPins = sizeof(pins) / sizeof(pins[0]);

int durationMs = 100;
int preActivationDelay = 0;
const int fixedPostActivationDelay = 1000;

// Large string handling variables
static bool largeStringActive = false;
static int expectedChunks = 0;
static int receivedChunks = 0;
static String largeStringBuffer = "";

// Struct for ESP-NOW messages
typedef struct struct_message {
  uint8_t command;
} struct_message;

struct_message incomingMessage;
struct_message confirmationMessage;

// Set all defined GPIO pins to HIGH or LOW
void setAllPins(bool state) {
  for (int i = 0; i < numPins; i++) {
    digitalWrite(pins[i], state ? HIGH : LOW);
  }
}

#include <ArduinoJson.h>

// Process received long string as JSON,  filters out data then hands off movement to another func
void processReceivedString() {
    Serial.println("Processing received large string...");

    StaticJsonDocument<3072> doc;  // Adjust buffer size as needed
    DeserializationError error = deserializeJson(doc, largeStringBuffer);

    if (error) {
        Serial.print("JSON Parsing Failed: ");
        Serial.println(error.c_str());
        return;
    }

    // Extracting values
    const char* stripeName = doc["stripeName"];
    float drop = doc["drop"];
    float startPulleyA = doc["startPulleyA"];
    float startPulleyB = doc["startPulleyB"];
    float stripeVelocity = doc["stripeVelocity"];

    // Extract pattern as a raw string
    String patternRaw = doc["pattern"].as<String>();

    // Clean up the pattern string (remove brackets, spaces, and extra quotes)
    patternRaw.replace("[", "");
    patternRaw.replace("]", "");
    patternRaw.replace("'", ""); // Remove single quotes
    patternRaw.replace("\"", ""); // Remove double quotes

    int alottedListSize = 150;

    // Convert the pattern string into a list
    String patternList[alottedListSize]; 
    int patternCount = 0;

    int start = 0;
    int end = patternRaw.indexOf(',');
   

    while (end != -1 && patternCount < alottedListSize) {
        patternList[patternCount] = patternRaw.substring(start, end);
        patternList[patternCount].trim(); // Remove any spaces
        patternCount++;
        start = end + 1;
        end = patternRaw.indexOf(',', start);
    }

    // Add last entry if available
    if (start < patternRaw.length() && patternCount < alottedListSize) {
        patternList[patternCount] = patternRaw.substring(start);
        patternList[patternCount].trim();
        patternCount++;
    }

    // Print extracted values
    Serial.println("Extracted Data:");
    Serial.print("Stripe Name: "); Serial.println(stripeName);
    Serial.print("Drop: "); Serial.println(drop, 4);
    Serial.print("Start Pulley A: "); Serial.println(startPulleyA, 4);
    Serial.print("Start Pulley B: "); Serial.println(startPulleyB, 4);
    Serial.print("Stripe Velocity: "); Serial.println(stripeVelocity, 4);
    
    Serial.print("Pattern Count: ");
    Serial.println(patternCount);

    Serial.println("Pattern List:");
    for (int i = 0; i < patternCount; i++) {
        Serial.print("  Entry "); Serial.print(i); Serial.print(": ");
        Serial.println(patternList[i]);
    }

  sprayAndStripe(stripeVelocity, drop, patternList, patternCount);
}

void sprayAndStripe(float stripeVelocity, float drop, String patternList[], int patternCount) {
    float movementTimeMs = (drop / stripeVelocity) * 1000;  // Convert seconds to milliseconds
    float timeBetweenSpraysMs = movementTimeMs / patternCount;

    Serial.print("Movement Time (ms): ");
    Serial.println(movementTimeMs);
    
    Serial.print("Time Between Sprays (ms): ");
    Serial.println(timeBetweenSpraysMs);

    
}

// Handle large string reception over ESP-NOW (from tested script)
void handleLargeStringPacket(const uint8_t *data, int len) {
  uint8_t packetType = data[0];

  if (packetType == 0x10) {
    expectedChunks = data[1] << 8 | data[2];
    receivedChunks = 0;
    largeStringBuffer = "";
    largeStringActive = true;

    Serial.print("Large string incoming; total chunks: ");
    Serial.println(expectedChunks);
  } 
  else if (packetType == 0x11 && largeStringActive) {
    int chunkIndex = data[1];
    const char* chunkContent = (const char*)&data[2];
    largeStringBuffer += String(chunkContent);
    receivedChunks++;

    Serial.print("Received chunk #");
    Serial.print(chunkIndex);
    Serial.print(" (");
    Serial.print(receivedChunks);
    Serial.print("/");
    Serial.print(expectedChunks);
    Serial.println(")");

    if (receivedChunks >= expectedChunks) {
      Serial.println("All chunks received! Reconstructed large string:");
      Serial.println(largeStringBuffer);

      struct_message confirmMsg;
      confirmMsg.command = 0x12; // Large string received confirmation
      esp_now_send(hubAddress, (uint8_t *)&confirmMsg, sizeof(confirmMsg));

      Serial.println("Large string confirmation sent to Hub.");
      largeStringActive = false;

      // Pass the received string to a function for future processing
      processReceivedString();
    }
  }
}

// ESP-NOW receive callback
void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  if (len >= 1) {
    handleLargeStringPacket(incomingData, len);
  }
}

// ESP-NOW send callback
void onDataSent(const uint8_t *macAddr, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void setup() {
  Serial.begin(115200);
  delay(5000);

  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  delay(1000); //trust
  Serial.print("MAC address: ");
  Serial.println(WiFi.macAddress());
  Serial.println("ESP-NOW Chassis Initialized");

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register ESP-NOW callbacks
  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  // Register peer (Hub)
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, hubAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  // Initialize GPIO pins
  for (int i = 0; i < numPins; i++) {
    pinMode(pins[i], OUTPUT);
  }

  randomSeed(analogRead(0));

  Serial.println("GPIO Control Script Initialized.");
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.equalsIgnoreCase("clean")) {
      Serial.println("Starting cleaning cycle...");
      for (int i = 0; i < 120; i++) {
        setAllPins(true);
        delay(500);
        setAllPins(false);
        delay(1000);
      }
      Serial.println("Cleaning cycle complete.");
    } 
    else if (input.startsWith("delay")) {
      int spaceIndex = input.indexOf(' ');
      if (spaceIndex != -1) {
        preActivationDelay = input.substring(spaceIndex + 1).toInt();
        Serial.print("Pre-activation delay set to: ");
        Serial.print(preActivationDelay);
        Serial.println(" ms");
      }
    } 
    else if (input.equalsIgnoreCase("forever")) {
      Serial.println("Starting forever cleaning cycle...");
      for (;;) { // Infinite loop with serial feedback
        Serial.println("Infinite loop running...");
        setAllPins(true);
        delay(1000);
        setAllPins(false);
        delay(30000);
      }
    } 
    else if (input.equalsIgnoreCase("rand")) {
      Serial.println("Starting random cycle...");
      delay(5000);
      for (int i = 0; i < 10; i++) {
        setAllPins(true);
        delay(durationMs);
        setAllPins(false);
        delay(random(300, 600));
      }
      Serial.println("Random cycle complete.");
    } 
    else if (input.equalsIgnoreCase("trig")) {
      Serial.println("Trigger command received.");
      delay(preActivationDelay);
      setAllPins(true);
      delay(durationMs);
      setAllPins(false);
      delay(fixedPostActivationDelay);
    } 
    else {
      int newDuration = input.toInt();
      if (newDuration > 0) {
        durationMs = newDuration;
        Serial.print("Activation duration set to: ");
        Serial.print(durationMs);
        Serial.println(" ms");
      }
    }
  }
}
