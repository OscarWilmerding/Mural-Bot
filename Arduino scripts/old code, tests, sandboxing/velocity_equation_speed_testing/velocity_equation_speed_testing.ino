/*
  ESP32 script to test the performance of the determineStripeVelocities() calculation.
  - Tries a few different (posA, posB) values
  - Times how long the calculation takes (in microseconds)
  - If the result is not valid (NaN, Inf, etc.), it skips without failing.
*/

#include <Arduino.h>

// Global pulley spacing for the calculation
float pulleySpacing = 1.0f;

// The function provided in the prompt (unchanged except for minor formatting)
void determineStripeVelocities(float posA, float posB, float &velA, float &velB) {
  float Vx = 0; // no movement in x direction
  float Vy = 1; // velocity in y direction is positive (down the wall)

  velA = pow(
            -pow(pulleySpacing, 4.0f)
            + (2.0f * posA * posA + 2.0f * posB * posB) * pulleySpacing * pulleySpacing
            - pow((posA - posB), 2.0f) * pow((posA + posB), 2.0f),
            -0.5f
         )
         * posA
         * (
           Vx * sqrt(
             -pow(pulleySpacing, 4.0f)
             + (2.0f * posA * posA + 2.0f * posB * posB) * pulleySpacing * pulleySpacing
             - pow((posA - posB), 2.0f) * pow((posA + posB), 2.0f)
           )
           - Vy * (posA * posA - posB * posB - pulleySpacing * pulleySpacing)
         )
         / pulleySpacing;

  velB = -posB
         * (
           Vx * sqrt(
             -pow(pulleySpacing, 4.0f)
             + (2.0f * posA * posA + 2.0f * posB * posB) * pulleySpacing * pulleySpacing
             - pow((posA - posB), 2.0f) * pow((posA + posB), 2.0f)
           )
           - Vy * (posA * posA - posB * posB + pulleySpacing * pulleySpacing)
         )
         * pow(
             -pow(pulleySpacing, 4.0f)
             + (2.0f * posA * posA + 2.0f * posB * posB) * pulleySpacing * pulleySpacing
             - pow((posA - posB), 2.0f) * pow((posA + posB), 2.0f),
             -0.5f
         )
         / pulleySpacing;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting velocity calculation timing tests...");

  // Sample test arrays for posA and posB
  // You can adjust these based on your needs
  float testA[5] = {0.5f, 1.0f, 2.0f, 2.5f, 3.0f};
  float testB[5] = {0.3f, 1.2f, 1.8f, 3.1f, 3.5f};

  for (int i = 0; i < 5; i++) {
    float posA = testA[i];
    float posB = testB[i];
    float velA = 0.0f;
    float velB = 0.0f;

    // Measure start time (microseconds)
    unsigned long startTime = micros();

    // Perform the calculation
    determineStripeVelocities(posA, posB, velA, velB);

    // Measure end time (microseconds)
    unsigned long endTime = micros();
    unsigned long duration = endTime - startTime;

    // Check whether the result is finite (not NaN or Inf)
    // If it failed, skip this test without crashing
    if (!isfinite(velA) || !isfinite(velB)) {
      Serial.print("Calculation failed for (posA, posB) = (");
      Serial.print(posA);
      Serial.print(", ");
      Serial.print(posB);
      Serial.println("). Skipping...");
      continue;
    }

    // Print timing and results
    Serial.print("Test #");
    Serial.print(i + 1);
    Serial.print(" | posA=");
    Serial.print(posA);
    Serial.print(", posB=");
    Serial.print(posB);
    Serial.print(" => velA=");
    Serial.print(velA);
    Serial.print(", velB=");
    Serial.print(velB);
    Serial.print(" | Time: ");
    Serial.print(duration);
    Serial.println(" us");
  }
}

void loop() {
  // Nothing special to do in the loop for this demo
}
