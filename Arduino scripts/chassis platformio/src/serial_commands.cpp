#include "hardware.h"

void printHelp() {
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

void handleSerialCommands() {
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
        else if (input.startsWith("calibration ")) {
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
        else if (input.startsWith("trig ")) {
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
            printHelp();
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
