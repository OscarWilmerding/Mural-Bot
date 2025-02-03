#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>

// Define Hub MAC address (you'll need to set this to the Hub's MAC address)
uint8_t hubAddress[] = {0x48, 0x27, 0xE2, 0xE6, 0xE6, 0x58};

// Servo and pin definitions
const int buttonPin = 3;       // GPIO pin connected to the button
const int potPin = 8;          // GPIO pin connected to the potentiometer
static const int servoPin = 9; // GPIO pin connected to the servo

Servo servo1;

// Struct for receiving and sending data
typedef struct struct_message {
  uint8_t command;
} struct_message;

// Received message
struct_message incomingMessage;

// Confirmation message
struct_message confirmationMessage;

// Variables for servo movement
int servoSpeed = 2;            // Servo speed (delay in milliseconds)
int delayAtBottom = 200;       // Delay at the bottom of the movement in ms

// Track last button state
int lastButtonState = HIGH;

static bool largeStringActive = false;
static int  expectedChunks    = 0;
static int  receivedChunks    = 0;
static String largeStringBuffer = "";

void handleLargeStringPacket(const uint8_t *data, int len) {
  // For simplicity, assume the first byte (data[0]) is a "type" indicator
  // 0x10 = "First Packet" (contains total chunks)
  // 0x11 = "Chunk Data"   (contains the actual chunk)
  
  uint8_t packetType = data[0];

  if (packetType == 0x10) {
    // This is the "first" packet telling us how many chunks to expect
    // For example, data[1..2] might store the total chunk count
    expectedChunks  = data[1] << 8 | data[2];  // read 2 bytes as int
    receivedChunks  = 0;
    largeStringBuffer = "";
    largeStringActive  = true;

    Serial.print("Large string incoming; total chunks: ");
    Serial.println(expectedChunks);

  } else if (packetType == 0x11 && largeStringActive) {
    // This is a chunk of the large string
    // data[1] could be chunkIndex, data[2..end] is chunk content
    int chunkIndex = data[1];

    // Copy the chunk content (starting at data[2])
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

    // If we've got them all, confirm to the sender
    if (receivedChunks >= expectedChunks) {
      Serial.println("All chunks received! Reconstructed large string:");
      Serial.println(largeStringBuffer);

      // Send a "large string confirmation" packet
      struct_message confirmMsg;
      confirmMsg.command = 0x12; // 0x12 means "large string received"
      esp_now_send(hubAddress, (uint8_t *)&confirmMsg, sizeof(confirmMsg));

      Serial.println("Large string confirmation sent to Hub.");
      largeStringActive = false; // reset
    }
  }
}

void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  // If the packet is our original simple command size, process as before
  if (len == sizeof(struct_message)) {
    memcpy(&incomingMessage, incomingData, sizeof(incomingMessage));

    // Handle your normal "servo command" etc.
    if (incomingMessage.command == 0x01) {
      Serial.println("Received command to execute servo movement");

      // Execute the servo movement
      servoMovement();

      // Send confirmation back to Hub
      confirmationMessage.command = 0x02;  // 0x02 represents "action completed"
      esp_now_send(hubAddress, (uint8_t *) &confirmationMessage, sizeof(confirmationMessage));
      Serial.println("Confirmation sent to Hub");
    }
  }
  // Otherwise, check if it’s a large string packet
  else {
    handleLargeStringPacket(incomingData, len);
  }
}

// Callback when data is sent
void onDataSent(const uint8_t *macAddr, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void setup() {
  Serial.begin(115200);

  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  Serial.println("ESP-NOW Chassis Initialized");

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register sending and receiving callbacks
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

  // Configure button pin as input with an internal pull-up resistor
  pinMode(buttonPin, INPUT_PULLUP);

  // Attach the servo to the servo pin
  servo1.attach(servoPin);

  // Set servo to resting position at 180 degrees
  servo1.write(180);
}

void servoMovement() {
  // Read the potentiometer value
  int potValue = analogRead(potPin);

  // Map the potentiometer value to an angle (e.g., 0-180 degrees)
  int angle = map(potValue, 0, 4095, 120, 180); // Adjust the range if necessary

  // Print the potentiometer value and mapped angle
  Serial.print("Potentiometer value: ");
  Serial.println(potValue);
  Serial.print("Mapped angle: ");
  Serial.println(angle);

  // Move the servo from 180 to the mapped angle
  for (int posDegrees = 180; posDegrees >= angle; posDegrees--) {
    servo1.write(posDegrees);
    Serial.print("Servo moving to: ");
    Serial.println(posDegrees);
    delay(servoSpeed); // Delay as per the servoSpeed variable
  }

  delay(delayAtBottom); // Optional delay at the bottom position

  // Move the servo back from the mapped angle to 180
  for (int posDegrees = angle; posDegrees <= 180; posDegrees++) {
    servo1.write(posDegrees);
    Serial.print("Servo returning to: ");
    Serial.println(posDegrees);
    delay(servoSpeed); // Delay as per the servoSpeed variable
  }

  Serial.println("Servo returned to resting position.");
}

void loop() {
  // Check if the button is pressed
  int buttonState = digitalRead(buttonPin);

  // Check for button press (LOW signal when pressed)
  if (buttonState == LOW && lastButtonState == HIGH) {
    Serial.println("Button pressed! Executing servo movement...");
    servoMovement();  // Trigger servo movement
  }

  // Update last button state
  lastButtonState = buttonState;


// SCRIPT FOR TESTING DOT CONSISTANCY, PUTS DOWN 30 DOTS WITH DELAY BETWEEN EACH DOT
// delay(5000);

// for (int i = 0; i < 30; i++) {
//     servoMovement();
//     delay(700); 
// }

// delay(30000);


}