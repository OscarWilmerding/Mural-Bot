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

// Callback when data is received (updated to match new signature)
void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  memcpy(&incomingMessage, incomingData, sizeof(incomingMessage));

  // Act based on command received (assuming 0x01 is the command to trigger servo movement)
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