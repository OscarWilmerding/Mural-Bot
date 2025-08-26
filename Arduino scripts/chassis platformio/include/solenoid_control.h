#pragma once

#include <Arduino.h>
#include "config.h"

void pullSolenoid(int solenoidNumber, int level);
void setAllPins(bool state);
void runCalibration(int solenoid, int lowMs, int highMs, int stepMs);
