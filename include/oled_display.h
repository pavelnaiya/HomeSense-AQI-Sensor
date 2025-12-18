#pragma once
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "pm_sensor.h"
#include "pin_configs.h"
#include "iaq_calculator.h"

class OLEDDisplay {
public:
    enum ScreenMode {
        AQI_SCREEN,
        PM25_SCREEN,
        PM10_SCREEN,
        TEMP_SCREEN,
        HUM_SCREEN,
        TVOC_SCREEN,
        CYCLE_ALL
    };

private:
    Adafruit_SSD1306 oled;
    uint8_t _sda, _scl, _addr;
    ScreenMode mode = CYCLE_ALL;
    uint8_t currentScreen = 0;
    unsigned long lastSwitch = 0;
    const unsigned long screenInterval = 2000; // 2 sec per screen

public:
    OLEDDisplay(uint8_t sda = OLED_SDA, uint8_t scl = OLED_SCL, uint8_t addr = OLED_ADDR)
        : oled(128, 32, &Wire, -1), _sda(sda), _scl(scl), _addr(addr) {}

    void begin() {
        Wire.begin(_sda, _scl);
        Wire.setClock(400000);

        if (!oled.begin(SSD1306_SWITCHCAPVCC, _addr)) {
            Serial.println("OLED init failed!");
            while (true) delay(1000);
        }

        oled.clearDisplay();
        oled.setTextSize(1);
        oled.setTextColor(SSD1306_WHITE);
    }

    void setMode(ScreenMode newMode) {
        mode = newMode;
        currentScreen = 0;
        lastSwitch = 0;
    }

    void showMessage(const char* msg) {
        oled.clearDisplay();
        oled.setCursor(0, 0);
        oled.println(msg);
        oled.display();
    }

    void show(uint16_t pm25, uint16_t pm10, float temp, float hum, float tvoc, int aqi, int batteryPercent) {
        oled.clearDisplay();
        
        // Draw Battery at top right
        drawBattery(batteryPercent);
        
        oled.setCursor(0, 0);

        // Handle auto-cycling
        if (mode == CYCLE_ALL) {
            unsigned long now = millis();
            if ((unsigned long)(now - lastSwitch) >= screenInterval) {
                currentScreen = (currentScreen + 1) % 6;
                lastSwitch = now;
            }
        }

        // Display based on mode or current screen
        int displayMode = (mode == CYCLE_ALL) ? currentScreen : mode;
        
        // Centered vertically for Size 2 font (32px height, font is ~16px high)
        // Cursor Y = 8 puts it perfectly in middle
        oled.setTextSize(2);
        oled.setCursor(0, 8);

        switch (displayMode) {
            case AQI_SCREEN:
                oled.setTextSize(2);
                oled.setCursor(0, 2);
                oled.printf("AQI:%d", aqi);
                oled.setTextSize(1);
                oled.setCursor(0, 22);
                oled.printf("Status:%s", IAQ::getAQICategory(aqi));
                break;
                
            case PM25_SCREEN:
                oled.printf("PM2.5:%d", pm25);
                oled.setTextSize(1);
                oled.setCursor(0, 24);
                oled.print("ug/m3");
                break;
                
            case PM10_SCREEN:
                oled.printf("PM10:%d", pm10);
                oled.setTextSize(1);
                oled.setCursor(0, 24);
                oled.print("ug/m3");
                break;
                
            case TEMP_SCREEN:
                oled.printf("Temp:%.0fC", temp);
                break;
                
            case HUM_SCREEN:
                oled.printf("Hum:%.0f%%", hum);
                break;
                
            case TVOC_SCREEN:
                oled.printf("TVOC:%.0f", tvoc);
                oled.setTextSize(1);
                oled.setCursor(0, 24);
                oled.print("PPB");
                break;
        }

        oled.display();
    }

    void drawBattery(int percent) {
        oled.setTextSize(1);
        oled.setTextColor(SSD1306_WHITE);
        oled.setCursor(95, 0);
        oled.printf("%d%%", percent);
        
        // Simple battery outline
        oled.drawRect(118, 0, 10, 6, SSD1306_WHITE);
        oled.drawRect(128, 2, 1, 2, SSD1306_WHITE); // Battery nipple
        
        // Fill based on percentage
        int fillWidth = (percent * 8) / 100;
        if (fillWidth > 0) {
            oled.fillRect(119, 1, fillWidth, 4, SSD1306_WHITE);
        }
    }

    // Optional: display all sensor data at once
    void showSensorDataFull(uint16_t pm25, float tvoc, float temp, float hum, int aqi, const char* aqiCategory = nullptr) {
        oled.clearDisplay();
        oled.setCursor(0, 0);
        oled.printf("PM2.5 : %d\n", pm25);
        oled.printf("TVOC  : %.1f\n", tvoc);
        oled.printf("Temp  : %.1f C\n", temp);
        oled.printf("Hum   : %.0f %%\n", hum);
        oled.printf("IAQ   : %d\n", aqi);
        if (aqiCategory) oled.printf("%s\n", aqiCategory);
        oled.display();
    }
};
