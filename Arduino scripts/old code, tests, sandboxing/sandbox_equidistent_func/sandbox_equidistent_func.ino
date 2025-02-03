void setup() {
  // put your setup code here, to run once:

}

void loop() {
  // put your main code here, to run repeatedly:

}


float baseAccelerationA = 2000.0*2.0;
float baseMaxSpeedA = 400.0*2.0;
float baseAccelerationB = 2000.0*2.0;
float baseMaxSpeedB = 400.0*2.0;

void move_to_position(float position1, float position2) {
  long currentPosA = stepper1.currentPosition();
  long currentPosB = stepper2.currentPosition();

  long targetPositionA = position1 * stepsPerMeter * motor1Direction;
  long targetPositionB = position2 * stepsPerMeter * motor2Direction;

  

  stepper1.moveTo(targetPosition1);
  stepper2.moveTo(targetPosition2);

  Serial.print("Moving to Position (m) - Motor 1: ");
  Serial.print(position1);
  Serial.print(" Motor 2: ");
  Serial.println(position2);

  movementInProgress = true;
}










