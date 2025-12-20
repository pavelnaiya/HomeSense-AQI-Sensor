#pragma once
#include <Wire.h>
#include <Adafruit_AGS02MA.h>
#include "config.h"

class TVOCSensor {
private:
    Adafruit_AGS02MA sensor;
    unsigned long warmupStart = 0;
    const unsigned long warmupTime = 120000; // 120 sec (per AGS02MA datasheet)

public:
    TVOCSensor() {}

    bool begin(uint8_t sda = AGS_SDA_PIN, uint8_t scl = AGS_SCL_PIN) {
        Wire.begin(sda, scl);
        Wire.setClock(25000L); // recommended for AGS02MA

        bool ok = sensor.begin();
        if (ok) warmupStart = millis();   // start warm-up timer
        return ok;
    }

    bool isWarmingUp() {
        return (millis() - warmupStart) < warmupTime;
    }

    float readTVOC() {
        if (isWarmingUp()) {
            return NAN;  // do not read until stable
        }

        float value = sensor.getTVOC();

        // library returns negative on failure
        if (value < 0) return NAN;

        return value;
    }
};
