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
        AQI_ONLY,
        PM_ONLY,
        TEMP_HUM_ONLY,
        TVOC_ONLY,
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

    void show(uint16_t pm25, uint16_t pm10, float temp, float hum, float tvoc, int aqi) {
        oled.clearDisplay();
        oled.setCursor(0, 0);

        // Handle auto-cycling
        if (mode == CYCLE_ALL) {
            unsigned long now = millis();
            if ((unsigned long)(now - lastSwitch) >= screenInterval) {
                currentScreen = (currentScreen + 1) % 4;
                lastSwitch = now;
            }
        }

        // Display based on mode or current screen
        switch (mode == CYCLE_ALL ? currentScreen : mode) {
            case AQI_ONLY: {
                const char* category = IAQ::getAQICategory(aqi);
                oled.printf("AQI: %d %s", aqi, category);
                break;
            }
            case PM_ONLY:
                oled.printf("PM2.5:%d PM10:%d", pm25, pm10);
                break;
            case TEMP_HUM_ONLY:
                oled.printf("T:%.1fC H:%.0f%%", temp, hum);
                break;
            case TVOC_ONLY:
                oled.printf("TVOC: %.1f ppb", tvoc);
                break;
        }

        oled.display();
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
