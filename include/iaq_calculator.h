#pragma once
#include <Arduino.h>
#include <math.h>

// -----------------------------
// Indoor Air Quality Calculator
// -----------------------------
namespace IAQ {

// ---------------------------------------------------------
// US EPA 2024 Breakpoints for PM2.5 (µg/m³)
// ---------------------------------------------------------
struct PM25Breakpoint {
    float low;
    float high;
    int aqiLow;
    int aqiHigh;
};

const PM25Breakpoint PM25_TABLE[] = {
    {0.0, 9.0, 0, 50},          // Good
    {9.1, 35.4, 51, 100},       // Moderate
    {35.5, 55.4, 101, 150},     // Unhealthy for sensitive
    {55.5, 125.4, 151, 200},    // Unhealthy (Updated 2024)
    {125.5, 225.4, 201, 300},   // Very Unhealthy (Updated 2024)
    {225.5, 500.4, 301, 500}    // Hazardous (Updated 2024)
};

// Breakpoints for PM10 (µg/m³) - Stays largely consistent
struct PM10Breakpoint {
    float low;
    float high;
    int aqiLow;
    int aqiHigh;
};

const PM10Breakpoint PM10_TABLE[] = {
    {0, 54, 0, 50},
    {55, 154, 51, 100},
    {155, 254, 101, 150},
    {255, 354, 151, 200},
    {355, 424, 201, 300},
    {425, 604, 301, 500}
};

// Helper: linear AQI interpolation
inline int interpolateAQI(float Cp, float Clow, float Chigh, int Ilow, int Ihigh) {
    if (Chigh == Clow) return Ilow;
    return round((float)(Ihigh - Ilow) / (Chigh - Clow) * (Cp - Clow) + Ilow);
}

// Calculate numeric AQI based on PM2.5
inline int calculateAQI_PM25(float pm25) {
    if (isnan(pm25)) return 0;
    if (pm25 < 0) return 0;
    for (int i = 0; i < 6; i++) {
        if (pm25 <= PM25_TABLE[i].high) {
            return interpolateAQI(pm25, PM25_TABLE[i].low, PM25_TABLE[i].high,
                                  PM25_TABLE[i].aqiLow, PM25_TABLE[i].aqiHigh);
        }
    }
    return 500;
}

// Calculate numeric AQI based on PM10
inline int calculateAQI_PM10(float pm10) {
    if (isnan(pm10)) return 0;
    if (pm10 < 0) return 0;
    for (int i = 0; i < 6; i++) {
        if (pm10 <= PM10_TABLE[i].high) {
            return interpolateAQI(pm10, PM10_TABLE[i].low, PM10_TABLE[i].high,
                                  PM10_TABLE[i].aqiLow, PM10_TABLE[i].aqiHigh);
        }
    }
    return 500;
}

/**
 * Calculate overall AQI from PM2.5 and PM10
 * Includes a calibration factor (0.85) to compensate for consumer sensor bias.
 */
inline int calculateAQI(float pm25, float pm10) {
    // Calibration factor: 0.85 compensates for over-reading in high humidity/low air flow 
    // common in budget PMS sensors.
    float calibratedPM25 = pm25 * 0.85f;
    float calibratedPM10 = pm10 * 0.85f;

    int aqi25 = calculateAQI_PM25(calibratedPM25);
    int aqi10 = calculateAQI_PM10(calibratedPM10);
    return max(aqi25, aqi10);
}

// Optional: adjust AQI with TVOC (scale 0–600 PPB to 0–100)
inline int adjustAQIWithTVOC(int baseAQI, float tvocPPB) {
    if (isnan(tvocPPB) || tvocPPB <= 0) return baseAQI;
    int tvocAQI = min(100, (int)round(tvocPPB / 6.0));
    return max(baseAQI, tvocAQI);
}

// Optional: convert AQI to category string
inline const char* getAQICategory(int aqi) {
    if (aqi <= 50) return "Good";
    if (aqi <= 100) return "Moderate";
    if (aqi <= 150) return "Unhealthy for Sensitive";
    if (aqi <= 200) return "Unhealthy";
    if (aqi <= 300) return "Very Unhealthy";
    return "Hazardous";
}

} // namespace IAQ
