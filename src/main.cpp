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
#include "battery_monitor.h"

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

    // Initialize OLED
    Serial.print("📺 Initializing OLED Display... ");
    display.begin();
    display.showMessage("HomeSense\nBooting...");
    Serial.println("Done");

    // Initialize sensors
    Serial.print("📡 Initializing PM Sensor... ");
    pm_sensor.begin(PM_RX_PIN, PM_TX_PIN);
    Serial.println("Done");

    Serial.print("🌡️  Initializing Temp/Humidity Sensor... ");
    temp_hum_sensor.begin();
    Serial.println("Done");

    Serial.print("☁️  Initializing TVOC Sensor... ");
    if (tvoc_sensor.begin(AGS_SDA_PIN, AGS_SCL_PIN)) {
        Serial.println("Done");
    } else {
        Serial.println("❌ TVOC sensor not found");
    }

    // WiFi Connection
    Serial.println("\n📶 Connecting to WiFi...");
    bool wifiConnected = connectWiFi();
    
    if (wifiConnected) {
        Serial.printf("✅ WiFi Connected: %s\n", WiFi.localIP().toString().c_str());
        display.showMessage("WiFi OK!");
        
        // Start web server with current WiFi credentials
        Serial.println("🌐 Starting Web Server...");
        web.begin(WiFi.SSID().c_str(), WiFi.psk().c_str());
        Serial.println("✅ Web Server Started");
        
        // Initialize OTA Updater
        WebUpdater::attach(server);
        Serial.println("✅ OTA Updater Ready at /update");
        
    } else {
        Serial.println("⚠️  WiFi connection failed - Starting AP mode");
        display.showMessage("WiFi Failed\nAP Mode");
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
    // Persistent Sensor Data (for UI redraws)
    static PMData lastValidPM = {0, 0, 0};
    static int pmReadFailures = 0;
    static float tvoc = 0;
    static float temp = 0;
    static float hum = 0;
    static int aqi = 0;
    static int batteryPercent = 0;

    // Non-blocking timer for sensors (10 seconds)
    static unsigned long lastSensorRead = 0;
    const unsigned long sensorInterval = 10000;

    if (millis() - lastSensorRead > sensorInterval) {
        lastSensorRead = millis();

        // Cloud upload handler
        web.loop();

        // Read PM sensor
        if (pm_sensor.read(pm)) {
            lastValidPM = pm;
            pmReadFailures = 0;
        } else {
            pm = lastValidPM;
            pmReadFailures++;
            if (pmReadFailures > 10) Serial.println("⚠️ PM Fail");
        }

        // Read other sensors
        tvoc = tvoc_sensor.readTVOC();
        temp = temp_hum_sensor.readTemperature();
        hum  = temp_hum_sensor.readHumidity();

        // Calculate AQI
        aqi = IAQ::calculateAQI(pm.pm2_5, pm.pm10);
        aqi = IAQ::adjustAQIWithTVOC(aqi, tvoc);
        const char* category = IAQ::getAQICategory(aqi);

        // Read battery
        batteryPercent = BatteryMonitor::getPercentage();

        // Serial Log
        Serial.printf(
            "📊 PM2.5:%3u | PM10:%3u | TVOC:%6.2f | Temp:%4.1f°C | Hum:%4.1f%% | AQI:%3d (%s) | Batt:%d%%\n",
            pm.pm2_5, pm.pm10, tvoc, temp, hum, aqi, category, batteryPercent
        );

        // Update OLED content
        display.show(pm.pm2_5, pm.pm10, temp, hum, tvoc, aqi, batteryPercent);
    }
    
    // ------------------------------------
    // UI Loop (Fast Response)
    // ------------------------------------
    static bool lastTouch = LOW;
    
    // Handle Touch Button instantly
    bool currentTouch = digitalRead(TOUCH_PIN);

    // Detect Rising Edge (LOW -> HIGH)
    if (currentTouch == HIGH && lastTouch == LOW) {
        // Cycle Mode
        switch (currentMode) {
            case OLEDDisplay::AQI_SCREEN:  currentMode = OLEDDisplay::PM25_SCREEN; break;
            case OLEDDisplay::PM25_SCREEN: currentMode = OLEDDisplay::PM10_SCREEN; break;
            case OLEDDisplay::PM10_SCREEN: currentMode = OLEDDisplay::TEMP_SCREEN; break;
            case OLEDDisplay::TEMP_SCREEN: currentMode = OLEDDisplay::HUM_SCREEN;  break;
            case OLEDDisplay::HUM_SCREEN:  currentMode = OLEDDisplay::TVOC_SCREEN; break;
            case OLEDDisplay::TVOC_SCREEN: currentMode = OLEDDisplay::CYCLE_ALL;   break;
            case OLEDDisplay::CYCLE_ALL:   currentMode = OLEDDisplay::AQI_SCREEN;  break;
        }
        display.setMode(currentMode);
        
        // Force redraw immediately using CACHED data
        display.show(pm.pm2_5, pm.pm10, temp, hum, tvoc, aqi, batteryPercent);
        
        // Simple debounce delay to prevent double-taps
        delay(200); 
    }
    
    lastTouch = currentTouch;

    // Reset watchdog timer
    esp_task_wdt_reset();
    delay(50); // Small delay for CPU breath
}
