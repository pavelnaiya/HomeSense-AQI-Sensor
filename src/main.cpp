#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>

#include "pin_configs.h"
#include "oled_display.h"
#include "pm_sensor.h"
#include "tvoc_sensor.h"
#include "temp_humidity_sensor.h"
#include "web_server.h"
#include "iaq_calculator.h"
#include "wifi_manager.h"
#include "web_updater.h"

// -----------------------------
// Module Instances
// -----------------------------
PMSensor pm_sensor(Serial2);
PMData pm;

TVOCSensor tvoc_sensor;
TempHumiditySensor temp_hum_sensor(DHT_PIN, DHT_TYPE);
OLEDDisplay display(OLED_SDA, OLED_SCL, OLED_ADDR);

AsyncWebServer server(80);
WebServerModule web(server, pm_sensor, tvoc_sensor, temp_hum_sensor);

OLEDDisplay::ScreenMode currentMode = OLEDDisplay::CYCLE_ALL;

// ======================================================================
// SETUP
// ======================================================================
void setup() {
    Serial.begin(115200);
    delay(200);

    Serial.println("\n==============================");
    Serial.println("      AQI Monitor Booting     ");
    Serial.println("==============================");

    pinMode(TOUCH_PIN, INPUT);

    // -----------------------------
    // Filesystem
    // -----------------------------
    if (!LittleFS.begin(true)) {
        Serial.println("❌ LittleFS mount failed");
    } else {
        Serial.println("✅ LittleFS mounted");
    }

    // -----------------------------
    // OLED
    // -----------------------------
    display.begin();
    display.showMessage("Booting...");

    // -----------------------------
    // Sensors
    // -----------------------------
    pm_sensor.begin(ZH07_RX_PIN, ZH07_TX_PIN);
    Serial.println("✅ PM Sensor initialized");

    if (tvoc_sensor.begin(AGS_SDA_PIN, AGS_SCL_PIN)) {
        Serial.println("✅ TVOC sensor initialized");
    } else {
        Serial.println("❌ TVOC sensor not found");
    }

    temp_hum_sensor.begin();
    Serial.println("✅ Temp/Humidity sensor initialized");

    display.showMessage("Sensors OK");

    // -----------------------------
    // WiFi Manager Integration
    // -----------------------------
    display.showMessage("WiFi Setup...");

    bool wifiConnected = connectWiFi();
    
    if (wifiConnected) {
        // Station mode - connected to WiFi
        Serial.printf("📶 WiFi connected: %s\n", WiFi.localIP().toString().c_str());
        display.showMessage("WiFi Ready");
        
        // Start web server with current WiFi credentials
        web.begin(WiFi.SSID().c_str(), WiFi.psk().c_str());
        
        // Initialize OTA Updater
        WebUpdater::attach(server);
        Serial.println("✅ OTA Updater Ready at /update");
        
    } else {
        // AP mode - start config portal
        Serial.println("⚠️ WiFi failed — starting AP mode...");
        display.showMessage("Setup Mode");
        startAPForConfig(&display);  // Pass display pointer for password display
        
        Serial.println("⚠️ In AP mode - web server not started for sensor data");
        Serial.println("📱 Connect to 'HomeSense-Setup' to configure WiFi");
    }

    // -----------------------------
    // Watchdog Timer
    // -----------------------------
    esp_task_wdt_init(30, true);  // 30 second timeout, panic on timeout
    esp_task_wdt_add(NULL);        // Add current task to watchdog
    Serial.println("✅ Watchdog timer enabled (30s timeout)");

    Serial.println("\n🚀 System Ready!");
    Serial.println("==============================");
}

// ======================================================================
// LOOP
// ======================================================================
void loop() {
    // Cloud upload handler
    web.loop();

    // Read PM sensor with error handling
    static PMData lastValidPM = {0, 0, 0};
    static int pmReadFailures = 0;
    
    if (pm_sensor.read(pm)) {
        // Successful read
        lastValidPM = pm;
        pmReadFailures = 0;
    } else {
        // Failed read - use last valid data
        pm = lastValidPM;
        pmReadFailures++;
        
        if (pmReadFailures > 10) {
            Serial.println("⚠️ PM Sensor: consecutive read failures");
        }
    }

    // Read TVOC, temp, hum
    float tvoc = tvoc_sensor.readTVOC();
    float temp = temp_hum_sensor.readTemperature();
    float hum  = temp_hum_sensor.readHumidity();

    // NAN protections
    if (isnan(tvoc)) tvoc = 0;
    if (isnan(temp)) temp = 0;
    if (isnan(hum))  hum  = 0;

    // Calculate AQI
    int aqi = IAQ::calculateAQI(pm.pm2_5, pm.pm10);
    aqi = IAQ::adjustAQIWithTVOC(aqi, tvoc);
    const char* category = IAQ::getAQICategory(aqi);

    // Serial Log
    Serial.printf(
        "📊 PM2.5:%3u | PM10:%3u | PM1.0:%3u | "
        "TVOC:%5.1f | Temp:%4.1f°C | Hum:%4.1f%% | "
        "AQI:%3d (%s)%s\n",
        pm.pm2_5, pm.pm10, pm.pm1_0,
        tvoc, temp, hum,
        aqi, category,
        (pmReadFailures > 0) ? " [STALE]" : ""
    );

    // Touch button → cycle modes
    static bool lastTouch = LOW;
    static unsigned long lastDebounceTime = 0;
    const unsigned long debounceDelay = 50;
    
    bool touch = digitalRead(TOUCH_PIN);

    if (touch != lastTouch) {
        lastDebounceTime = millis();
    }
    
    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (touch == HIGH && lastTouch == LOW) {
            switch (currentMode) {
                case OLEDDisplay::AQI_ONLY:      currentMode = OLEDDisplay::PM_ONLY; break;
                case OLEDDisplay::PM_ONLY:       currentMode = OLEDDisplay::TEMP_HUM_ONLY; break;
                case OLEDDisplay::TEMP_HUM_ONLY: currentMode = OLEDDisplay::TVOC_ONLY; break;
                case OLEDDisplay::TVOC_ONLY:     currentMode = OLEDDisplay::CYCLE_ALL; break;
                case OLEDDisplay::CYCLE_ALL:     currentMode = OLEDDisplay::AQI_ONLY; break;
            }
            display.setMode(currentMode);
        }
    }
    lastTouch = touch;

    // Update OLED
    display.show(pm.pm2_5, pm.pm10, temp, hum, tvoc, aqi);

    // Reset watchdog timer
    esp_task_wdt_reset();

    delay(1000);
}
