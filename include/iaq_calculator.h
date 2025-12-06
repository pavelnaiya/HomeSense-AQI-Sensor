#pragma once
#include <Arduino.h>
#include <math.h>

// -----------------------------
// Indoor Air Quality Calculator
// -----------------------------
namespace IAQ {

// Breakpoints for PM2.5 (µg/m³) based on US EPA standard
struct PM25Breakpoint {
    float low;
    float high;
    int aqiLow;
    int aqiHigh;
};

const PM25Breakpoint PM25_TABLE[] = {
    {0.0, 12.0, 0, 50},       // Good
    {12.1, 35.4, 51, 100},    // Moderate
    {35.5, 55.4, 101, 150},   // Unhealthy for sensitive
    {55.5, 150.4, 151, 200},  // Unhealthy
    {150.5, 250.4, 201, 300}, // Very Unhealthy
    {250.5, 500.0, 301, 500}  // Hazardous
};

// Breakpoints for PM10 (µg/m³)
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
    return round((Ihigh - Ilow) / (Chigh - Clow) * (Cp - Clow) + Ilow);
}

// Calculate numeric AQI based on PM2.5
inline int calculateAQI_PM25(float pm25) {
    if (isnan(pm25)) return 0; // treat invalid as 0
    for (int i = 0; i < 6; i++) {
        if (pm25 >= PM25_TABLE[i].low && pm25 <= PM25_TABLE[i].high) {
            return interpolateAQI(pm25, PM25_TABLE[i].low, PM25_TABLE[i].high,
                                  PM25_TABLE[i].aqiLow, PM25_TABLE[i].aqiHigh);
        }
    }
    return -1; // out of range
}

// Calculate numeric AQI based on PM10
inline int calculateAQI_PM10(float pm10) {
    if (isnan(pm10)) return 0;
    for (int i = 0; i < 6; i++) {
        if (pm10 >= PM10_TABLE[i].low && pm10 <= PM10_TABLE[i].high) {
            return interpolateAQI(pm10, PM10_TABLE[i].low, PM10_TABLE[i].high,
                                  PM10_TABLE[i].aqiLow, PM10_TABLE[i].aqiHigh);
        }
    }
    return -1;
}

// Calculate overall AQI from PM2.5 and PM10
inline int calculateAQI(float pm25, float pm10) {
    int aqi25 = calculateAQI_PM25(pm25);
    int aqi10 = calculateAQI_PM10(pm10);
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
