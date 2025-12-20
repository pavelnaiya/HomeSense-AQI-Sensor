#pragma once
#include <Arduino.h>
#include "pin_configs.h"

class BatteryMonitor {
private:
    static constexpr float EMA_ALPHA = 0.2f; // Smoothing factor (0.1 to 0.3 is good)
    
    // ADC calibration factor: measured voltage / calculated voltage
    // To calibrate:
    // 1. Measure voltage at GPIO34 pin with multimeter (e.g., 2.08V)
    // 2. Read the raw ADC value and calculate: (rawAvg / 4095.0) * 3.3
    // 3. Calibration factor = measured_voltage / calculated_voltage
    // Example: If multimeter shows 2.08V but ADC calculates 1.8875V,
    //          then ADC_CALIBRATION = 2.08 / 1.8875 = 1.102
    // For BAK NMC N18650CL-29 battery (2.08V measured at pin, battery 4.26V):
    // - Expected at pin: 4.26V / 2 = 2.13V
    // - Actual at pin: 2.08V (slight difference, possibly resistor tolerance)
    // - If code shows 50%, it's calculating ~3.775V battery (~1.8875V at pin)
    // - Calibration needed: 2.08 / 1.8875 ≈ 1.102
    static constexpr float ADC_CALIBRATION = 1.102f; // Calibrated: 2.08V measured / 1.8875V calculated ≈ 1.102

public:
    static float readVoltage() {
        // Function-local static variable for smoothed voltage (avoids C++17 inline requirement)
        static float smoothedVoltage = -1.0f;
        
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
        if (smoothedVoltage < 0) {
            smoothedVoltage = currentVoltage; // First reading
        } else {
            smoothedVoltage = (currentVoltage * EMA_ALPHA) + (smoothedVoltage * (1.0f - EMA_ALPHA));
        }

        return smoothedVoltage;
    }

    static int getPercentage() {
        float voltage = readVoltage();
        
        // BAK NMC N18650CL-29 3.6V 2900mAh Li-ion battery with TP4056 charging module
        // TP4056 Protection Circuit Specifications:
        // - Over-discharge protection: 2.4V ± 100mV (battery cuts off)
        // - Over-discharge release: 3.0V ± 100mV (battery becomes usable again)
        // - Charging voltage: 4.2V ± 1% (standard full charge)
        // - Overcharge protection: 4.3V ± 50mV (protection activates)
        // Battery specs: 4.2V fully charged, 2.5V fully discharged (absolute minimum)
        
        // 100% = 4.2V (standard full charge), allow up to 4.3V (overcharge protection limit)
        if (voltage >= 4.20f) return 100;
        // 0% = 3.0V (over-discharge release voltage - battery usable again after protection)
        if (voltage <= 3.00f) return 0;
        
        // Map 3.0V -> 4.2V to 0% -> 100%
        // Linear mapping (actual discharge curve is slightly non-linear, but this is a good approximation)
        float percent = (voltage - 3.00f) / (4.20f - 3.00f) * 100.0f;
        
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
