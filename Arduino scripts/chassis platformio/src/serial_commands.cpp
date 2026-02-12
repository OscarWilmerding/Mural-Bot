#include "hardware.h"

void printHelp() {
    Serial.println(F("=== Available Serial Commands ==="));
    Serial.println(F("clean                – waterfall cleaning cycle (all solenoids, 20x each, 100ms between)"));
    Serial.println(F("delay <ms>           – set pre activation delay (fractional ms allowed)"));
    Serial.println(F("forever              – endless clean pulses"));
    Serial.println(F("rand                 – 10 random pulses"));
    Serial.println(F("trig                 – trigger ALL pins once"));
    Serial.println(F("trig <S>,<C>,<D>     – pulse solenoid S, C times; D=downtime ms between shots (fractional ms allowed)"));
    Serial.println(F("<number>             – set pulse width (ms) (fractional allowed, e.g. 2.45)"));
    Serial.println(F("calibration <solenoid>,<low>,<high>,<step> – sweep pulse widths"));
    Serial.println(F("heater <1|2|both> <0-100>% – set heater PWM duty cycle"));
    Serial.println(F("?                    – show this help list"));
}

void processCommand(String input) {
    input.trim();

    if (input.equalsIgnoreCase("clean")) {
        Serial.println("Starting waterfall cleaning cycle...");
        unsigned long pulseMs = (unsigned long)(durationMs + 0.5f);
        
        // 20 cycles through all solenoids
        for (int cycle = 0; cycle < 20; cycle++) {
            // Trigger each solenoid in sequence
            for (int sol = 1; sol <= NUM_SOLENOIDS; sol++) {
                pullSolenoidForUs(sol, (unsigned long)(durationMs * 1000.0f + 0.5f));
                // Wait 100ms after pulse completes before triggering next solenoid
                delay(100);
            }
        }
        Serial.println("Cleaning cycle complete.");
    }
    else if (input.startsWith("calibration ")) {
        String args = input.substring(12);
        int first  = args.indexOf(',');
        int second = args.indexOf(',', first  + 1);
        int third  = args.indexOf(',', second + 1);

        if (first > 0 && second > first && third > second) {
            int sol      = args.substring(0, first).toInt();
            float low    = args.substring(first  + 1, second).toFloat();
            float high   = args.substring(second + 1, third).toFloat();
            float step   = args.substring(third  + 1).toFloat();

            Serial.print("Starting calibration sweep on solenoid "); Serial.println(sol);
            runCalibration(sol, low, high, step);
        } else {
            Serial.println("calibration syntax error");
        }
    }
    else if (input.startsWith("delay")) {
        int spaceIndex = input.indexOf(' ');
        if (spaceIndex != -1) {
            preActivationDelay = input.substring(spaceIndex + 1).toFloat();
            Serial.print("Pre-activation delay set to: ");
            Serial.print(preActivationDelay, 3);
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
            unsigned long usec = (unsigned long)(durationMs * 1000.0f + 0.5f);
            if (usec > 0) delayMicroseconds(usec);
            setAllPins(false);
            delay(random(300, 600));
        }
        Serial.println("Random cycle complete.");
    }
    else if (input.startsWith("trig ")) {
        // Expect: trig <solenoid>,<count>,<downtime_ms>
        String args = input.substring(5);
        int firstComma = args.indexOf(',');
        int secondComma = args.indexOf(',', firstComma + 1);
        
        if (firstComma == -1 || secondComma == -1) {
            Serial.println("Syntax: trig <solenoid>,<count>,<downtime_ms>");
        } else {
            int solenoidNum = args.substring(0, firstComma).toInt();
            int repeatCnt = args.substring(firstComma + 1, secondComma).toInt();
            float downtimeMs = args.substring(secondComma + 1).toFloat();

            if (solenoidNum >= 1 && solenoidNum <= NUM_SOLENOIDS && repeatCnt > 0) {
                Serial.print("Pulsing solenoid "); Serial.print(solenoidNum);
                Serial.print(" for "); Serial.print(repeatCnt); Serial.print(" time(s) with "); Serial.print(downtimeMs,3); Serial.println(" ms downtime");

                if (preActivationDelay) delay((unsigned long)(preActivationDelay + 0.5f));

                for (int i = 0; i < repeatCnt; i++) {
                    unsigned long usec = (unsigned long)(durationMs * 1000.0f + 0.5f);
                    pullSolenoidForUs(solenoidNum, usec);
                    // use downtime between shots (off-time) instead of fixedPostActivationDelay
                    if (downtimeMs > 0.0f) delay((unsigned long)(downtimeMs + 0.5f));
                }
                // After sequence, enforce standard post activation behavior
                delay(fixedPostActivationDelay);
            } else {
                Serial.println("Invalid solenoid # or count.");
            }
        }
    }
    else if (input.equalsIgnoreCase("trig")) {
        Serial.println("Trigger command received.");
        if (preActivationDelay) delay((unsigned long)(preActivationDelay + 0.5f));
        setAllPins(true);
        unsigned long usecAll = (unsigned long)(durationMs * 1000.0f + 0.5f);
        if (usecAll > 0) delayMicroseconds(usecAll);
        setAllPins(false);
        delay(fixedPostActivationDelay);
    }
    else if (input == "?") {
        printHelp();
    }
    else if (input.startsWith("heater ") || input.startsWith("HEATER ")) {
        // Parse: heater <1|2|both> <0-100>%
        String args = input.substring(7);
        int spaceIndex = args.indexOf(' ');
        
        if (spaceIndex == -1) {
            Serial.println("Syntax: heater <1|2|both> <0-100>%");
            return;
        }
        
        String heaterTarget = args.substring(0, spaceIndex);
        heaterTarget.toLowerCase();
        String dutyCycleStr = args.substring(spaceIndex + 1);
        
        // Remove '%' if present
        if (dutyCycleStr.endsWith("%")) {
            dutyCycleStr = dutyCycleStr.substring(0, dutyCycleStr.length() - 1);
        }
        
        int dutyCycle = dutyCycleStr.toInt();
        
        // Validate duty cycle
        if (dutyCycle < 0 || dutyCycle > 100) {
            Serial.println("Error: duty cycle must be 0-100%");
            return;
        }
        
        // Apply to specified heater(s)
        if (heaterTarget == "1") {
            setHeaterPWM(1, dutyCycle);
            Serial.print("Heater 1 set to ");
            Serial.print(dutyCycle);
            Serial.println("%");
        }
        else if (heaterTarget == "2") {
            setHeaterPWM(2, dutyCycle);
            Serial.print("Heater 2 set to ");
            Serial.print(dutyCycle);
            Serial.println("%");
        }
        else if (heaterTarget == "both") {
            setHeaterPWM(1, dutyCycle);
            setHeaterPWM(2, dutyCycle);
            Serial.print("Both heaters set to ");
            Serial.print(dutyCycle);
            Serial.println("%");
        }
        else {
            Serial.println("Error: heater must be '1', '2', or 'both'");
        }
    }
    else {
        float newDuration = input.toFloat();
        if (newDuration > 0.0f) {
            durationMs = newDuration;
            Serial.print("Activation duration set to: ");
            Serial.print(durationMs, 3);
            Serial.println(" ms");
        }
    }
}

void handleSerialCommands() {
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        processCommand(input);
    }
}
