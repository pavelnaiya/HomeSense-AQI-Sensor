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

    void showBootAnimation(const char* version = "1.0.3") {
        oled.clearDisplay();
        
        // Step 1: Show "HomeSense" title
        oled.setTextSize(2);
        oled.setTextColor(SSD1306_WHITE);
        oled.setCursor(10, 8);
        oled.print("HomeSense");
        oled.display();
        delay(400);
        
        // Step 2: Show version info
        oled.setTextSize(1);
        oled.setCursor(35, 24);
        oled.printf("v%s", version);
        oled.display();
        delay(400);
        
        // Step 3: Animated loading dots (3 dots bouncing)
        for (int cycle = 0; cycle < 2; cycle++) {
            for (int i = 0; i < 3; i++) {
                // Clear all dots
                oled.fillRect(50, 20, 30, 6, SSD1306_BLACK);
                // Draw dots up to current position
                for (int j = 0; j <= i; j++) {
                    oled.fillRect(50 + j * 10, 20, 6, 6, SSD1306_WHITE);
                }
                oled.display();
                delay(150);
            }
        }
        
        // Step 4: Progress bar animation
        for (int i = 0; i <= 100; i += 4) {
            oled.fillRect(0, 28, 128, 4, SSD1306_BLACK); // Clear progress bar
            oled.drawRect(0, 28, 128, 4, SSD1306_WHITE); // Draw border
            int barWidth = (i * 126) / 100; // 126 to account for border
            if (barWidth > 0) {
                oled.fillRect(1, 29, barWidth, 2, SSD1306_WHITE); // Fill progress
            }
            oled.display();
            delay(25);
        }
        
        delay(200);
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

    // Firmware update animation
    void showUpdateAnimation(const char* newVersion = nullptr, int progress = -1) {
        oled.clearDisplay();
        
        // Title
        oled.setTextSize(1);
        oled.setTextColor(SSD1306_WHITE);
        oled.setCursor(20, 0);
        oled.print("FIRMWARE UPDATE");
        oled.display();
        
        // Version info if provided
        if (newVersion) {
            oled.setCursor(25, 10);
            oled.setTextSize(1);
            oled.printf("v%s", newVersion);
        }
        
        // Progress bar (if progress >= 0)
        if (progress >= 0 && progress <= 100) {
            oled.drawRect(10, 20, 108, 8, SSD1306_WHITE);
            int barWidth = (progress * 106) / 100; // 106 to account for border
            if (barWidth > 0) {
                oled.fillRect(11, 21, barWidth, 6, SSD1306_WHITE);
            }
            // Show percentage
            oled.setCursor(50, 22);
            oled.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Inverted text
            oled.printf("%d%%", progress);
            oled.setTextColor(SSD1306_WHITE); // Reset
        } else {
            // Animated dots while waiting
            static int dotPos = 0;
            oled.setCursor(45, 22);
            for (int i = 0; i < 3; i++) {
                if (i == dotPos) {
                    oled.print("O");
                } else {
                    oled.print(".");
                }
            }
            dotPos = (dotPos + 1) % 3;
        }
        
        oled.display();
    }
};
