#include <esp_now.h>
#include <WiFi.h>
#include <ArduinoJson.h>

// -------------------- Peer MAC --------------------
uint8_t hubAddress[] = {0x48, 0x27, 0xE2, 0xE6, 0xE6, 0x58};

// -------------------- Pins (single source of truth) --------------------
constexpr uint8_t SOLENOID_PINS[] = {
  17, // 1
  21, // 2
  22, // 3
  25, // 4
  32, // 5
  15, // 6
  33, // 7
  27, // 8
  4,  // 9
  16, // 10
  26, // 11
  14, // 12
  13, // 13 (extra port)
  12  // 14 (buzzer)
};
constexpr int NUM_SOLENOIDS = sizeof(SOLENOID_PINS) / sizeof(SOLENOID_PINS[0]);

// -------------------- Timing and config --------------------
constexpr int MAX_LEDGER_SIZE = 100;

int MAX_LEDGER_SIZE_UNUSED__remove_if_seen = MAX_LEDGER_SIZE; // keeps intent visible if you search

int durationMs = 100;              // serial "trig" pulse width
int preActivationDelay = 0;        // used by serial commands
const int fixedPostActivationDelay = 1000;

// -------------------- Long message reassembly --------------------
static bool   largeStringActive   = false;
static int    expectedChunks      = 0;
static int    receivedChunks      = 0;
static String largeStringBuffer;

// Process deferral after all chunks arrive
bool newMessageReady = false;

// -------------------- ESP-NOW message struct --------------------
typedef struct struct_message {
  uint8_t command;
  uint8_t chunkIndex;
} struct_message;

struct_message incomingMessage;
struct_message confirmationMessage;

// -------------------- Helpers: pins --------------------
inline void pullSolenoid(int solenoidNumber, int level) {
  if (solenoidNumber < 1 || solenoidNumber > NUM_SOLENOIDS) return;
  digitalWrite(SOLENOID_PINS[solenoidNumber - 1], level);
}

void setAllPins(bool state) {
  for (int i = 0; i < NUM_SOLENOIDS; i++) {
    digitalWrite(SOLENOID_PINS[i], state ? HIGH : LOW);
  }
}

// -------------------- Ledger-based scheduler --------------------
struct ledgerEntry {
  int      solenoid;        // 1..NUM_SOLENOIDS; -1 means empty
  uint32_t triggerTimeMs;   // absolute millis when to go HIGH
  uint16_t pulseWidthMs;    // width for this event
  bool     triggered;       // whether we've already gone HIGH
};

ledgerEntry ledger[MAX_LEDGER_SIZE];

void initLedger() {
  for (int i = 0; i < MAX_LEDGER_SIZE; i++) {
    ledger[i] = { -1, 0, 0, false };
  }
}

void schedulePin(int solenoid, uint32_t delayFromNowMs, uint16_t widthMs = durationMs) {
  if (solenoid < 1 || solenoid > NUM_SOLENOIDS) return;
  const uint32_t t = millis() + delayFromNowMs;

  // Simple coalescing
  for (int i = 0; i < MAX_LEDGER_SIZE; i++) {
    if (ledger[i].solenoid == solenoid && !ledger[i].triggered) {
      uint32_t a = ledger[i].triggerTimeMs, b = t;
      uint32_t diff = (a > b) ? (a - b) : (b - a);
      if (diff < widthMs) return; // close enough, don't double-schedule
    }
  }
  // Insert
  for (int i = 0; i < MAX_LEDGER_SIZE; i++) {
    if (ledger[i].solenoid == -1) {
      ledger[i] = { solenoid, t, widthMs, false };
//      Serial.printf("SCHEDULED: solenoid %d in %lu ms (at %lu)\n", solenoid, (unsigned long)delayFromNowMs, (unsigned long)t);
      return;
    }
  }

  Serial.printf("WARNING: Ledger full. Could not schedule solenoid %d at %lu ms\n",
                solenoid, (unsigned long)t);
}


// -------------------- Pattern interpreter --------------------
void interpretPattern(const String& patternToProcess, float speed /*units per second*/) {
  if (patternToProcess.length() < 4 || speed <= 0.0f) return;
  auto ms = [](float seconds) -> uint32_t { return (uint32_t)(seconds * 1000.0f + 0.5f); };

  for (int i = 0; i < 4; i++) {
    char c = patternToProcess[i];
    if (c == 'x') continue;

    if (c == '1') {
      // your intentional offsets for 2 and 4 (kept)
      if (i == 0) { schedulePin(1, 1); }
      if (i == 1) { schedulePin(2, ms(0.1f / speed)); }
      if (i == 2) { schedulePin(3, 1); }
      if (i == 3) { schedulePin(4, ms(0.1f / speed)); }
    } else if (c == '2') {
      // 5..8 with same delay
      schedulePin(5 + i, ms(0.1f / speed));
    } else if (c == '3') {
      // 9..12 with same delay
      schedulePin(9 + i, ms(0.2f / speed));
    }
  }
}

// -------------------- Calibration helper --------------------
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

// -------------------- Spray + Stripe state (blocking version) --------------------
void sprayAndStripe(float stripeVelocity, float drop, String* patternList, int patternCount) {
  initLedger();                       // clear any residual entries

  if (stripeVelocity <= 0.0f || patternCount <= 0) return;

  const double movementTimeMs = (double)drop / (double)stripeVelocity * 1000.0;
  const double intervalMs = movementTimeMs / (double)patternCount;

  const uint32_t startMs = millis();
  double nextTriggerAt = (double)startMs + intervalMs;
  int triggerCount = 0;

  Serial.print("Total Movement Time: "); Serial.print(movementTimeMs); Serial.println(" ms");
  Serial.print("Interval between sprays: "); Serial.print(intervalMs); Serial.println(" ms");
  Serial.println("-- STARTING SPRAY STRIPE SOLENOID MOVEMENTS --");

  while ((double)(millis() - startMs) < movementTimeMs) {
    const uint32_t now = millis();

    if (triggerCount < patternCount && (double)now >= nextTriggerAt) {
      interpretPattern(patternList[triggerCount++], stripeVelocity);
      nextTriggerAt += intervalMs;
    }

    for (int i = 0; i < MAX_LEDGER_SIZE; i++) {
      if (ledger[i].solenoid == -1) continue;

      if (!ledger[i].triggered && now >= ledger[i].triggerTimeMs) {
        pullSolenoid(ledger[i].solenoid, HIGH);
//        Serial.printf("SOLENOID HIGH: %d at %lu ms, width=%u ms\n",ledger[i].solenoid, (unsigned long)millis(), (unsigned)ledger[i].pulseWidthMs);//COMMENT THIS LINE DURING PROPER RUNNING IT WILL SLOW SHIT DOWN
        ledger[i].triggered = true;
      } else if (ledger[i].triggered && now >= ledger[i].triggerTimeMs + ledger[i].pulseWidthMs) {
        pullSolenoid(ledger[i].solenoid, LOW);
        ledger[i].solenoid = -1; // free the slot
      }
    }
    // no delay(1) — keep loop tight for lower jitter
  }

  Serial.println("Movement complete, turning off spray solenoids.");
  for (int s = 1; s <= 12; s++) pullSolenoid(s, LOW); // keep original behavior
}

// -------------------- JSON processing --------------------
void processReceivedString() {
  Serial.println("Processing received large string...");
  Serial.println("Full received data:");
  Serial.println(largeStringBuffer);

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

// -------------------- ESP-NOW chunked receive --------------------
void handleLargeStringPacket(const uint8_t *data, int len) {
  if (len < 1) return;
  uint8_t packetType = data[0];

  if (packetType == 0x10) {
    expectedChunks = (data[1] << 8) | data[2];
    receivedChunks = 0;
    // reserve enough capacity. mirror CHUNK_PAYLOAD_SIZE from sender (200)
    largeStringBuffer.reserve(expectedChunks * 200 + 1);
    largeStringBuffer = "";
    largeStringActive = true;

    // ACK start
    struct_message startAck;
    startAck.command    = 0x10;      // start-ack
    startAck.chunkIndex = 0;
    esp_now_send(hubAddress, (uint8_t *)&startAck, sizeof(startAck));

    Serial.print("Large string incoming; total chunks: ");
    Serial.println(expectedChunks);
    return;
  }

  if (packetType == 0x11 && largeStringActive) {
    const uint8_t chunkIndex = data[1];

    // safer append using known length
    int payloadLen = len - 2;             // type + index already consumed
    if (payloadLen > 0 && data[2 + payloadLen - 1] == '\0') {
      payloadLen--;                       // drop trailing null if present
    }
    largeStringBuffer.concat((const char*)&data[2], payloadLen);

    receivedChunks++;

    // per-chunk ack
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

// -------------------- ESP-NOW callbacks --------------------
void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  if (len >= 1) handleLargeStringPacket(incomingData, len);
}

void onDataSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

// -------------------- Arduino setup/loop --------------------
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

  for (int i = 0; i < NUM_SOLENOIDS; i++) {
    pinMode(SOLENOID_PINS[i], OUTPUT);
    digitalWrite(SOLENOID_PINS[i], LOW);
  }

  initLedger();

  randomSeed(analogRead(0));
  Serial.println("GPIO Control Script Initialized.");
}

void loop() {
  // Handle deferred message parsing
  if (newMessageReady) {
    newMessageReady = false;
    processReceivedString();
  }

  // Serial command interface
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
    else if (input.startsWith("calibration ")) {   // calibration 3,10,20,2
      String args = input.substring(12);
      int first  = args.indexOf(',');
      int second = args.indexOf(',', first  + 1);
      int third  = args.indexOf(',', second + 1);

      if (first > 0 && second > first && third > second) {
        int sol  = args.substring(0, first).toInt();
        int low  = args.substring(first  + 1, second).toInt();
        int high = args.substring(second + 1, third).toInt();
        int step = args.substring(third  + 1).toInt();

        Serial.printf("Starting calibration sweep on solenoid %d\n", sol);
        runCalibration(sol, low, high, step);
      } else {
        Serial.println("calibration syntax error");
      }
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
    else if (input.startsWith("trig ")) {          // trig <solenoid>,<count>
      int commaPos = input.indexOf(',');
      if (commaPos == -1) {
        Serial.println("Syntax: trig <solenoid>,<count>");
      } else {
        int solenoidNum = input.substring(5, commaPos).toInt();
        int repeatCnt   = input.substring(commaPos + 1).toInt();

        if (solenoidNum >= 1 && solenoidNum <= NUM_SOLENOIDS && repeatCnt > 0) {
          Serial.printf("Pulsing solenoid %d for %d time(s)\n", solenoidNum, repeatCnt);

          if (preActivationDelay) delay(preActivationDelay);

          for (int i = 0; i < repeatCnt; i++) {
            pullSolenoid(solenoidNum, HIGH);
            delay(durationMs);
            pullSolenoid(solenoidNum, LOW);
            delay(fixedPostActivationDelay);
          }
        } else {
          Serial.println("Invalid solenoid # or count.");
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
      Serial.println(F("clean                – 120 shot cleaning cycle"));
      Serial.println(F("delay <ms>           – set pre activation delay"));
      Serial.println(F("forever              – endless clean pulses"));
      Serial.println(F("rand                 – 10 random pulses"));
      Serial.println(F("trig                 – trigger ALL pins once"));
      Serial.println(F("trig <S>,<C>         – pulse solenoid S, C times"));
      Serial.println(F("<number>             – set pulse width (ms)"));
      Serial.println(F("calibration <solenoid>,<low>,<high>,<step> – sweep pulse widths"));
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
