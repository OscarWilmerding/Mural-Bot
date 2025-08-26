// Hi! Why don't scientists trust atoms? Because they make up everything!

#include <Arduino.h>
#include "LittleFS.h"
#include "state.h"
#include "modules.h"

void loadCommandsFromFile(const char *path) {
  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  bool readingStripeBlock = false;
  Command tempCmd;

  Serial.println("Loading Commands from File:");
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();

    if (line.startsWith("//") || line.length() == 0) continue;

    if (line.startsWith("number of drawn columns =")) {
      int eqIndex = line.indexOf('=');
      if (eqIndex >= 0) {
        String val = line.substring(eqIndex + 1);
        val.trim();
        numberOfDrawnColumns = val.toInt();
        Serial.print("Parsed number of drawn columns: ");
        Serial.println(numberOfDrawnColumns);
      }
      continue;
    }

    if (line.startsWith("pulley spacing =")) {
      int eqIndex = line.indexOf('=');
      if (eqIndex >= 0) {
        String val = line.substring(eqIndex + 1);
        val.trim();
        pulleySpacing = val.toFloat();
        Serial.print("Parsed pulley spacing: ");
        Serial.println(pulleySpacing);
      }
      continue;
    }

    if (line.startsWith("STRIPE - column #")) {
      readingStripeBlock = true;
      tempCmd.type         = Command::STRIPE;
      tempCmd.stripeName   = line;
      tempCmd.drop         = 0.0;
      tempCmd.startPulleyA = 0.0;
      tempCmd.startPulleyB = 0.0;
      tempCmd.pattern      = "";
      Serial.print("Detected STRIPE block: ");
      Serial.println(tempCmd.stripeName);
      continue;
    }

    if (readingStripeBlock) {
      if (line.startsWith("starting/ending position pixel values:")) continue;

      if (line.startsWith("drop:")) {
        float d = line.substring(5).toFloat();
        tempCmd.drop = d;
        Serial.print("Parsed drop: ");
        Serial.println(d);
        continue;
      }

      if (line.startsWith("pattern:")) {
        int colonIndex = line.indexOf(':');
        if (colonIndex >= 0) {
          String patternData = line.substring(colonIndex + 1);
          patternData.trim();
          tempCmd.pattern = patternData;
          Serial.println("Parsed pattern array for stripe.");
          Serial.println("pattern data raw being loaded into command struct:" + tempCmd.pattern);
        }
        continue;
      }

      if (line.startsWith("starting pulley values:")) {
        int colonIndex = line.indexOf(':');
        if (colonIndex >= 0) {
          String sub = line.substring(colonIndex + 1);
          sub.trim();
          int comma = sub.indexOf(',');
          if (comma >= 0) {
            String valA = sub.substring(0, comma);
            String valB = sub.substring(comma + 1);
            valA.trim(); valB.trim();
            tempCmd.startPulleyA = valA.toFloat();
            tempCmd.startPulleyB = valB.toFloat();

            commands[commandCount++] = tempCmd;
            Serial.println("Finished STRIPE command. Added to array.");
            readingStripeBlock = false;
          }
        }
        continue;
      }
    }

    if (line.startsWith("change color to:")) {
      Command cmd;
      cmd.type = Command::COLOR_CHANGE;
      cmd.colorHex = line.substring(16);
      cmd.colorHex.trim();
      cmd.colorHex.toUpperCase();
      commands[commandCount++] = cmd;
    } else {
      Serial.print("UNRECOGNIZED COMMANDS IN GCODE FILE");
    }
  }

  file.close();
  Serial.print("Total commands loaded: ");
  Serial.println(commandCount);
}
