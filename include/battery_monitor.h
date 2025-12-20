#pragma once
#include <Arduino.h>
#include "pin_configs.h"

class BatteryMonitor {
private:
    static constexpr float EMA_ALPHA = 0.2f; // Smoothing factor (0.1 to 0.3 is good)
    static inline float _smoothedVoltage = -1.0f;
    
    // ADC calibration factor: measured voltage / calculated voltage
    // To calibrate:
    // 1. Measure voltage at GPIO34 pin with multimeter (e.g., 2.08V)
    // 2. Read the raw ADC value and calculate: (rawAvg / 4095.0) * 3.3
    // 3. Calibration factor = measured_voltage / calculated_voltage
    // Example: If multimeter shows 2.08V but ADC calculates 1.8875V,
    //          then ADC_CALIBRATION = 2.08 / 1.8875 = 1.102
    // For your case (2.08V measured at pin, battery 4.26V):
    // - Expected at pin: 4.26V / 2 = 2.13V
    // - Actual at pin: 2.08V (slight difference, possibly resistor tolerance)
    // - If code shows 50%, it's calculating ~3.775V battery (~1.8875V at pin)
    // - Calibration needed: 2.08 / 1.8875 ≈ 1.102
    static constexpr float ADC_CALIBRATION = 1.102f; // Calibrated: 2.08V measured / 1.8875V calculated ≈ 1.102

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
        
        // Apply ADC calibration if needed
        voltageAtPin *= ADC_CALIBRATION;
        
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
        
        // Li-ion Discharge curve (Typical 18650/3.7V cell)
        // 4.2V = 100% (fully charged), 3.0V = 0% (fully discharged)
        // Using 3.3V as safe cutoff to prevent over-discharge
        if (voltage >= 4.20f) return 100;
        if (voltage <= 3.30f) return 0;
        
        // Map 3.3V -> 4.2V to 0% -> 100%
        float percent = (voltage - 3.30f) / (4.20f - 3.30f) * 100.0f;
        
        // Clamp result
        if (percent > 100) percent = 100;
        if (percent < 0) percent = 0;
        
        return (int)percent;
    }
    
    // Helper method to calculate ADC calibration factor
    // Call this with: measured voltage at GPIO34 pin (from multimeter)
    // Returns the calibration factor to use for ADC_CALIBRATION
    static float calculateCalibrationFactor(float measuredPinVoltage) {
        // Read raw ADC value
        long sum = 0;
        const int samples = 64;
        for (int i = 0; i < samples; i++) {
            sum += analogRead(BATTERY_PIN);
            delayMicroseconds(50);
        }
        float rawAvg = (float)sum / samples;
        float calculatedVoltage = (rawAvg / 4095.0f) * 3.3f;
        
        if (calculatedVoltage > 0) {
            return measuredPinVoltage / calculatedVoltage;
        }
        return 1.0f;
    }
};
