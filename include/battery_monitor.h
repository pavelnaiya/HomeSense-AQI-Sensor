#pragma once
#include <Arduino.h>
#include "pin_configs.h"

class BatteryMonitor {
public:
    static float readVoltage() {
        // ESP32 ADC is 12-bit (0-4095)
        // Default attenuation (11dB) allows reading up to ~3.3V (approx 3.1-3.2V actual)
        // We read raw value, convert to voltage at pin, then multiply by divider ratio
        int raw = analogRead(BATTERY_PIN);
        
        // Convert to voltage (3.3V reference / 4095 steps)
        // Note: ESP32 ADC isn't perfectly linear, but for a battery gauge it's fine.
        float voltageAtPin = (raw / 4095.0f) * 3.3f;
        
        return voltageAtPin * VOLT_DIVIDER_RATIO;
    }

    static int getPercentage() {
        float voltage = readVoltage();
        
        // Lio-ion Discharge curve (Simplified)
        if (voltage >= 4.2f) return 100;
        if (voltage <= 3.3f) return 0;
        
        // Linear map between 3.3V and 4.2V
        float percent = (voltage - 3.3f) / (4.2f - 3.3f) * 100.0f;
        
        return (int)percent;
    }
};
