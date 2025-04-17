#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>

// Define Hub MAC address
uint8_t hubAddress[] = {0x48, 0x27, 0xE2, 0xE6, 0xE6, 0x58};

// GPIO pin setup for serial commands
const int pins[] = {17, 21, 22, 25, 32, 15, 33, 27, 4, 16, 26, 14, 13, 12};
const int numPins = sizeof(pins) / sizeof(pins[0]);
const unsigned long PIN_HIGH_DURATION_MS = 12; 
int MAX_LEDGER_SIZE = 50;

int durationMs = 100;
int preActivationDelay = 0;
const int fixedPostActivationDelay = 1000;

// Large string handling variables
static bool largeStringActive = false;
static int expectedChunks = 0;
static int receivedChunks = 0;
static String largeStringBuffer;

// <-- Added/Modified: New global flag
bool newMessageReady = false;

// Struct for ESP-NOW messages
typedef struct struct_message {
  uint8_t command;
  uint8_t chunkIndex;
} struct_message;

struct_message incomingMessage;
struct_message confirmationMessage;

void setAllPins(bool state) {
  for (int i = 0; i < numPins; i++) {
    digitalWrite(pins[i], state ? HIGH : LOW);
  }
}

#include <ArduinoJson.h>

// Process received long string as JSON
void processReceivedString() {
    Serial.println("Processing received large string...");
    Serial.println("Full received data:");
    Serial.println(largeStringBuffer);

    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, largeStringBuffer);

    // Free up memory after parsing
    String tempBuffer = largeStringBuffer;
    largeStringBuffer = "";

    if (error) {
        Serial.print("JSON Parsing Failed: ");
        Serial.println(error.c_str());
        return;
    }

    const char* stripeName = doc["stripeName"];
    float drop = doc["drop"];
    float startPulleyA = doc["startPulleyA"];
    float startPulleyB = doc["startPulleyB"];
    float stripeVelocity = doc["stripeVelocity"];

    JsonArray patternArray = doc["pattern"].as<JsonArray>();
    int patternCount = patternArray.size();

    String* patternList = new String[patternCount];
    int i = 0;
    Serial.println("starting to fill pattern array with json data");
    for (JsonVariant v : patternArray) {
        if (i < patternCount) {
            patternList[i++] = v.as<String>();
        } else {
            break;
        }
    }

    Serial.println("Extracted Data:");
    Serial.print("Stripe Name: "); Serial.println(stripeName);
    Serial.print("Drop: "); Serial.println(drop, 4);
    Serial.print("Start Pulley A: "); Serial.println(startPulleyA, 4);
    Serial.print("Start Pulley B: "); Serial.println(startPulleyB, 4);
    Serial.print("Stripe Velocity: "); Serial.println(stripeVelocity, 4);
    Serial.print("Pattern Count: "); Serial.println(patternCount);

    sprayAndStripe(stripeVelocity, drop, patternList, patternCount);
    delete[] patternList;
}

////////////////////////////////////////////////////
struct ledgerEntry {
  int pin;
  unsigned long triggerTime;  
  bool triggered;             
};

ledgerEntry ledger[50]; 

void initLedger() {
  for (int i = 0; i < 50; i++) {
    ledger[i] = {-1, 0, false};
  }
}

void schedulePin(int pin, unsigned long delayFromNow) {
  unsigned long triggerAt = millis() + delayFromNow;
  for (int i = 0; i < 50; i++) {
    if (ledger[i].pin == -1) {
      ledger[i] = {pin, triggerAt, false};
      break;
    }
  }
}

void interpretPattern(String patternToProcess, unsigned long currentMillis, int speed) {
  for (int i = 0; i < 4; i++) {
    if (patternToProcess[i] == 'x') continue;

    Serial.print("Position ");
    Serial.print(i + 1);
    Serial.print(": ");

    if (patternToProcess[i] == '1') {
      if (i == 0) { schedulePin(1, 1); }
      if (i == 1) { schedulePin(2, 1); }
      if (i == 2) { schedulePin(3, 1); }
      if (i == 3) { schedulePin(4, 1); }
    } else if (patternToProcess[i] == '2') {
      if (i == 0) { schedulePin(5, (0.1 / speed) * 1000); }
      if (i == 1) { schedulePin(6, (0.1 / speed) * 1000); }
      if (i == 2) { schedulePin(7, (0.1 / speed) * 1000); }
      if (i == 3) { schedulePin(8, (0.1 / speed) * 1000); }
    } else if (patternToProcess[i] == '3') {
      if (i == 0) { schedulePin(9, (0.2 / speed) * 1000); }
      if (i == 1) { schedulePin(10, (0.2 / speed) * 1000); }
      if (i == 2) { schedulePin(11, (0.2 / speed) * 1000); }
      if (i == 3) { schedulePin(12, (0.2 / speed) * 1000); }
    }
  }
}

const int solenoidPins[14] = {
  17, // Solenoid 1
  21, // Solenoid 2
  22, // Solenoid 3
  25, // Solenoid 4
  32, // Solenoid 5
  15, // Solenoid 6
  33, // Solenoid 7
  27, // Solenoid 8
  4,  // Solenoid 9
  16, // Solenoid 10
  26, // Solenoid 11
  14, // Solenoid 12
  13, // Extra port out
  12  // Buzzer
};

void pullSolenoid(int solenoidNumber, int condition) {
  if (solenoidNumber < 1 || solenoidNumber > 14) return; 
  int gpio = solenoidPins[solenoidNumber - 1];
  digitalWrite(gpio, condition);
}

void sprayAndStripe(float stripeVelocity, float drop, String* patternList, int patternCount) {
  float movementTimeMs = (drop / stripeVelocity) * 1000;
  float timeBetweenSpraysMs = movementTimeMs / patternCount;

  Serial.print("Total Movement Time: ");
  Serial.print(movementTimeMs);
  Serial.println(" ms");
  Serial.print("Interval between sprays: ");
  Serial.print(timeBetweenSpraysMs);
  Serial.println(" ms");

  unsigned long startMillis = millis();
  unsigned long lastTriggerMillis = startMillis;
  int triggerCount = 0;

  Serial.print("-- STARTING SPRAY STRIPE SOLENOID MOVEMENTS --");

  while (millis() - startMillis < movementTimeMs) {
    delay(1); 

    unsigned long currentMillis = millis();

    if (currentMillis - lastTriggerMillis >= timeBetweenSpraysMs && triggerCount < patternCount) {
      interpretPattern(patternList[triggerCount++], currentMillis, stripeVelocity);
      lastTriggerMillis += timeBetweenSpraysMs;
    }

    for (int i = 0; i < MAX_LEDGER_SIZE; i++) {
      if (ledger[i].pin != -1) {
        if (!ledger[i].triggered && currentMillis >= ledger[i].triggerTime) {
          pullSolenoid(ledger[i].pin, HIGH);
          ledger[i].triggered = true;
        } else if (ledger[i].triggered && currentMillis >= ledger[i].triggerTime + PIN_HIGH_DURATION_MS) {
          pullSolenoid(ledger[i].pin, LOW);
          ledger[i].pin = -1;
        }
      }
    }
  }

  Serial.println("Movement complete, turning off all solenoids.");
  for (int i = 1; i <= 12; i++) {
    pullSolenoid(i, LOW);
  }
}


// Handle large string reception
void handleLargeStringPacket(const uint8_t *data, int len) {
  uint8_t packetType = data[0];

  if (packetType == 0x10) {
    expectedChunks = data[1] << 8 | data[2];
    receivedChunks = 0;
    largeStringBuffer.reserve(expectedChunks * 32); 
    largeStringBuffer = "";
    largeStringActive = true;

    Serial.print("Large string incoming; total chunks: ");
    Serial.println(expectedChunks);
  } 
  else if (packetType == 0x11 && largeStringActive) {
    int chunkIndex = data[1];
    largeStringBuffer += String((const char*)&data[2]);
    receivedChunks++;

    Serial.print("Received chunk #");
    Serial.print(chunkIndex);
    Serial.print(" (");
    Serial.print(receivedChunks);
    Serial.print("/");
    Serial.print(expectedChunks);
    Serial.println(")");

    // Send chunk-level ack
    struct_message chunkAck;
    chunkAck.command    = 0x13;
    chunkAck.chunkIndex = chunkIndex; 
    esp_now_send(hubAddress, (uint8_t *)&chunkAck, sizeof(chunkAck));
    
    // If all chunks arrived, send final confirmation and defer parsing
    if (receivedChunks >= expectedChunks) {
      Serial.println("All chunks received!");
      struct_message confirmMsg;
      confirmMsg.command = 0x12;
      confirmMsg.chunkIndex = 0;
      esp_now_send(hubAddress, (uint8_t *)&confirmMsg, sizeof(confirmMsg));

      Serial.println("Large string confirmation sent to Hub.");
      largeStringActive = false;

      // <-- Added/Modified: Do NOT call processReceivedString() here.
      // Instead, set flag to handle it in loop().
      newMessageReady = true; // <-- Added/Modified
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
  delay(2000);

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

  for (int i = 0; i < numPins; i++) {
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], LOW);
  }

  initLedger();

  randomSeed(analogRead(0));
  Serial.println("GPIO Control Script Initialized.");
}

void loop() {
  // <-- Added/Modified: If new message is ready, process now
  if (newMessageReady) {         // <-- Added/Modified
    newMessageReady = false;     // <-- Added/Modified
    processReceivedString();     // <-- Added/Modified
  }

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
      for (;;) {
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
    else if (input.startsWith("trig ")) {            // includes trailing space
      int commaPos = input.indexOf(',');
      if (commaPos == -1) {
        Serial.println("Syntax: trig <solenoid>,<count>");
      } else {
        int solenoidNum = input.substring(5, commaPos).toInt();     // after "trig "
        int repeatCnt   = input.substring(commaPos + 1).toInt();

        if (solenoidNum >= 1 && solenoidNum <= 14 && repeatCnt > 0) {
          Serial.printf("Pulsing solenoid %d for %d time(s)\n",
                       solenoidNum, repeatCnt);

          if (preActivationDelay) delay(preActivationDelay);

          for (int i = 0; i < repeatCnt; i++) {
            pullSolenoid(solenoidNum, HIGH);   // fire
            delay(durationMs);                 // pulse width
            pullSolenoid(solenoidNum, LOW);    // release
            delay(fixedPostActivationDelay);   // gap between pulses
          }
        } else {
          Serial.println("Invalid solenoid # (1‑14) or count (>0).");
        }
      }
    }
    else if (input.equalsIgnoreCase("trig")) {
      Serial.println("Trigger command received.");
      delay(preActivationDelay);
      setAllPins(true);
      delay(durationMs);
      setAllPins(false);
      delay(fixedPostActivationDelay);
    } 
    else if (input == "?") {
      Serial.println(F("=== Available Serial Commands ==="));
      Serial.println(F("clean                – 120‑shot cleaning cycle"));
      Serial.println(F("delay <ms>           – set pre‑activation delay"));
      Serial.println(F("forever              – endless clean pulses"));
      Serial.println(F("rand                 – 10 random pulses"));
      Serial.println(F("trig                 – trigger ALL pins once"));
      Serial.println(F("trig <S>,<C>         – pulse solenoid S, C times"));
      Serial.println(F("<number>             – set pulse width (ms)"));
      Serial.println(F("?                    – show this help list"));
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
