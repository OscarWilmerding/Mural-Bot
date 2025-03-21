#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>

// Define Hub MAC address
uint8_t hubAddress[] = {0x48, 0x27, 0xE2, 0xE6, 0xE6, 0x58};

// GPIO pin setup for serial commands
const int pins[] = {17, 21, 22, 25, 32, 15, 33, 27, 4, 16, 26, 14, 13, 12};
const int numPins = sizeof(pins) / sizeof(pins[0]);
const unsigned long PIN_HIGH_DURATION_MS = 12; // or any desired duration


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

////////////////////////////////////////////////////
struct ledgerEntry {
  int pin;
  unsigned long triggerTime;  
  bool triggered;             
};

ledgerEntry ledger[50]; // adjust size as needed

// Initialize ledger entries as inactive
void initLedger() {
  for (int i = 0; i < 50; i++) {
    ledger[i] = {-1, 0, false};
  }
}

// Schedule a pin trigger at a specified delay from now
void schedulePin(int pin, unsigned long delayFromNow) {
  unsigned long triggerAt = millis() + delayFromNow;
  for (int i = 0; i < 50; i++) {
    if (ledger[i].pin == -1) { // empty slot
      ledger[i] = {pin, triggerAt, false};
      break;
    }
  }
}


void interpretPattern(String patternToProcess, unsigned long currentMillis, int speed) {
  for (int i = 0; i < 4; i++) {
    if (patternToProcess[i] == 'x') continue;  // Skip 'x'

    Serial.print("Position ");
    Serial.print(i + 1);
    Serial.print(": ");

    if (patternToProcess[i] == '1') {
      if (i == 0) {
        schedulePin(1, 1);
      }
      if (i == 1) {
        schedulePin(2, 1); 
      }
      if (i == 2) {
        schedulePin(3, 1); 
      }
      if (i == 3) {
        schedulePin(4, 1); 
      }
    } else if (patternToProcess[i] == '2') {
      if (i == 0) {
        schedulePin(5, (0.1 / speed) * 1000);
      }
      if (i == 1) {
        schedulePin(6, (0.1 / speed) * 1000); 
      }
      if (i == 2) {
        schedulePin(7, (0.1 / speed) * 1000); 
      }
      if (i == 3) {
        schedulePin(8, (0.1 / speed) * 1000); 
      }
    } else if (patternToProcess[i] == '3') {
      if (i == 0) {
        schedulePin(9, (0.2 / speed) * 1000); 
      }
      if (i == 1) {
        schedulePin(10, (0.2 / speed) * 1000); 
      }
      if (i == 2) {
        schedulePin(11, (0.2 / speed) * 1000); 
      }
      if (i == 3) {
        schedulePin(12, (0.2 / speed) * 1000); 
      }
    }
  }
}

// Map solenoid numbers (1-12) to arbitrary GPIO pins
// Solenoid-to-GPIO mapping (1–12)
const int solenoidPins[14] = {
  18, // Solenoid 1
  23, // Solenoid 2
  19, // Solenoid 3
  25, // Solenoid 4
  32, // Solenoid 5
  15, // Solenoid 6
  33, // Solenoid 7
  27, // Solenoid 8
  4,  // Solenoid 9
  16, // Solenoid 10
  26, // Solenoid 11
  14  // Solenoid 12
  13, // Extra port out
  12  // Buzzer
};

void pullSolenoid(int solenoidNumber, int condition) {
  if (solenoidNumber < 1 || solenoidNumber > 14) return; // Guard clause
  int gpio = solenoidPins[solenoidNumber - 1];           // Convert to 0-based index
  digitalWrite(gpio, condition);
}

void sprayAndStripe(float stripeVelocity, float drop, String patternList[], int patternCount) {
  float movementTimeMs = (drop / stripeVelocity) * 1000;  // Convert seconds to milliseconds
  float timeBetweenSpraysMs = movementTimeMs / patternCount;

  Serial.print("Movement Time (ms): ");
  Serial.println(movementTimeMs);
  
  Serial.print("Time Between Sprays (ms): ");
  Serial.println(timeBetweenSpraysMs);

  unsigned long startMillis = millis();
  unsigned long lastTriggerMillis = startMillis;
  unsigned long currentMillis = millis();
  int triggerCount = 0;

  while (true) {  
    currentMillis = millis();

    // Exit condition
    if (currentMillis - startMillis >= movementTimeMs) {
      break;
    }

    // Time for next trigger
    if (currentMillis - lastTriggerMillis >= timeBetweenSpraysMs) {
      interpretPattern(patternList[triggerCount++], currentMillis, stripeVelocity);
      lastTriggerMillis += timeBetweenSpraysMs;
    }

    //CHECKING THE LEDGER FOR PINS TO TURN ON
    for (int i = 0; i < 50; i++) {
      if (ledger[i].pin != -1) {
        if (!ledger[i].triggered && currentMillis >= ledger[i].triggerTime) {
          pullSolenoid(ledger[i].pin, HIGH);  //TODO remap this to the right pins
          ledger[i].triggered = true;
        } 
        else if (ledger[i].triggered && currentMillis >= ledger[i].triggerTime + PIN_HIGH_DURATION_MS) {
          pullSolenoid(ledger[i].pin, LOW);
          ledger[i].pin = -1;  // mark as inactive
        }
      }
    }
  }

  // Ensure all pins off at end
  for (int i = 0; i < 12; i++) {
    pullSolenoid(i, LOW);
  }
}



///////////////////////////////////////////////

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
