#pragma once
#include <Arduino.h>
#include "pin_configs.h"

class BatteryMonitor {
private:
    static constexpr float EMA_ALPHA = 0.2f; // Smoothing factor (0.1 to 0.3 is good)
    static inline float _smoothedVoltage = -1.0f;

public:
    static float readVoltage() {
        // Multi-sampling: Read 64 samples to average out high-frequency noise
        long sum = 0;
        const int samples = 64;
        
        for (int i = 0; i < samples; i++) {
            sum += analogRead(BATTERY_PIN);
            delayMicroseconds(50); // Small pause for ADC stability
        }
        
        float rawAvg = (float)sum / samples;
        
        // Convert to voltage (3.3V reference / 4095 steps)
        // Note: ESP32 ADC is non-linear especially at the extremes.
        // For 100k+100k divider on VBAT (up to 4.2V), pin is ~2.1V which is in the linear range.
        float voltageAtPin = (rawAvg / 4095.0f) * 3.3f;
        float currentVoltage = voltageAtPin * VOLT_DIVIDER_RATIO;

        // Apply Exponential Moving Average (EMA) to filter out jumps from voltage sag
        if (_smoothedVoltage < 0) {
            _smoothedVoltage = currentVoltage; // First reading
        } else {
            _smoothedVoltage = (currentVoltage * EMA_ALPHA) + (_smoothedVoltage * (1.0f - EMA_ALPHA));
        }

        return _smoothedVoltage;
    }

    static int getPercentage() {
        float voltage = readVoltage();
        
        // Lio-ion Discharge curve (Simplified but slightly more realistic)
        // 4.2V = 100%, 3.4V = 0% (3.3V is critically low)
        if (voltage >= 4.15f) return 100;
        if (voltage <= 3.40f) return 0;
        
        // Map 3.4V -> 4.15V to 0% -> 100%
        float percent = (voltage - 3.40f) / (4.15f - 3.40f) * 100.0f;
        
        // Clamp result
        if (percent > 100) percent = 100;
        if (percent < 0) percent = 0;
        
        return (int)percent;
    }
};
