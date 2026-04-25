#include "hardware.h"
#include "core.h"

void printHelp() {
    Serial.println(F("=== Available Serial Commands ==="));
    Serial.println(F("clean                – waterfall cleaning cycle (all solenoids, 20x each, 100ms between)"));
    Serial.println(F("delay <ms>           – set pre activation delay (fractional ms allowed)"));
    Serial.println(F("forever              – endless clean pulses"));
    Serial.println(F("rand                 – 10 random pulses"));
    Serial.println(F("trig                 – trigger ALL pins once"));
    Serial.println(F("trig <S>,<C>,<D>     – pulse solenoid S, C times; D=downtime ms between shots (fractional ms allowed)"));
    Serial.println(F("<number>             – set pulse width for ALL solenoids (ms) (fractional allowed, e.g. 2.45)"));
    Serial.println(F("solenoid <N> <dur>   – set pulse width for solenoid N only (fractional allowed)"));
    Serial.println(F("calibration <solenoid>,<low>,<high>,<step> – sweep pulse widths"));
    Serial.println(F("heater <1|2|both> <0-100>% – set heater PWM duty cycle"));
    Serial.println(F("dump                 – print last stripe diagnostic log"));
    Serial.println(F("?                    – show this help list"));
}

void processCommand(String input) {
    input.trim();

    if (input.equalsIgnoreCase("clean")) {
        Serial.println("Starting waterfall cleaning cycle...");
        
        // 20 cycles through all solenoids
        for (int cycle = 0; cycle < 20; cycle++) {
            // Trigger each solenoid in sequence
            for (int sol = 1; sol <= NUM_SOLENOIDS; sol++) {
                unsigned long usec = (unsigned long)(getActualDuration(sol) * 1000.0f + 0.5f);
                pullSolenoidForUs(sol, usec);
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
            // Trigger all solenoids with their individual durations
            for (int sol = 1; sol <= NUM_SOLENOIDS; sol++) {
                unsigned long usec = (unsigned long)(getActualDuration(sol) * 1000.0f + 0.5f);
                pullSolenoidForUs(sol, usec);
                delay(100);
            }
        }
    }
    else if (input.equalsIgnoreCase("rand")) {
        Serial.println("Starting random cycle...");
        delay(5000);
        for (int i = 0; i < 10; i++) {
            // Trigger all solenoids with their individual durations
            for (int sol = 1; sol <= NUM_SOLENOIDS; sol++) {
                unsigned long usec = (unsigned long)(getActualDuration(sol) * 1000.0f + 0.5f);
                pullSolenoidForUs(sol, usec);
            }
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
                    unsigned long usec = (unsigned long)(getActualDuration(solenoidNum) * 1000.0f + 0.5f);
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
        
        // Trigger all solenoids with their individual durations
        for (int sol = 1; sol <= NUM_SOLENOIDS; sol++) {
            unsigned long usec = (unsigned long)(getActualDuration(sol) * 1000.0f + 0.5f);
            pullSolenoidForUs(sol, usec);
        }
        delay(fixedPostActivationDelay);
    }
    else if (input.equalsIgnoreCase("dump")) {
        Serial.print("DIAG: "); Serial.println(diagLog);
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
    else if (input.startsWith("solenoid ")) {
        String args = input.substring(9);  // Skip "solenoid "
        int spaceIndex = args.indexOf(' ');
        
        if (spaceIndex == -1) {
            Serial.println("Syntax: solenoid <N> <duration>");
            return;
        }
        
        int solenoidNum = args.substring(0, spaceIndex).toInt();
        float duration = args.substring(spaceIndex + 1).toFloat();
        
        if (solenoidNum >= 1 && solenoidNum <= NUM_SOLENOIDS && duration > 0.0f) {
            solenoidDurationMs[solenoidNum - 1] = duration;
            Serial.print("Solenoid ");
            Serial.print(solenoidNum);
            Serial.print(" pulse width set to: ");
            Serial.print(duration, 3);
            Serial.println(" ms");
        } else {
            Serial.println("Error: invalid solenoid number or duration");
        }
    }
    else {
        float newDuration = input.toFloat();
        if (newDuration > 0.0f) {
            // Set global and reset all per-solenoid durations
            durationMs = newDuration;
            for (int i = 0; i < NUM_SOLENOIDS; i++) {
                solenoidDurationMs[i] = newDuration;
            }
            Serial.print("Activation duration set to: ");
            Serial.print(durationMs, 3);
            Serial.println(" ms (all solenoids)");
        }
    }
}

void handleSerialCommands() {
    if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        processCommand(input);
    }
}
