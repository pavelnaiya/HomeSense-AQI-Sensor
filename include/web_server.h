#pragma once

#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "pm_sensor.h"
#include "tvoc_sensor.h"
#include "temp_humidity_sensor.h"
#include "iaq_calculator.h"

class WebServerModule {
private:
    AsyncWebServer &server;
    PMSensor &pm_sensor;
    TVOCSensor &tvoc_sensor;
    TempHumiditySensor &temp_hum_sensor;

    unsigned long uploadIntervalMs = 30000;  // Default 30 seconds
    unsigned long lastUploadTime = 0;
    String apiEndpoint = "https://home-sense.vercel.app/api/aqi";  // Default endpoint

    bool asyncPost = true;

    // Load configuration from LittleFS
    void loadConfig() {
        if (!LittleFS.exists("/config.json")) {
            Serial.println("⚠️ config.json not found, using defaults");
            return;
        }

        File file = LittleFS.open("/config.json", "r");
        if (!file) {
            Serial.println("⚠️ Failed to open config.json");
            return;
        }

        StaticJsonDocument<512> doc;
        DeserializationError err = deserializeJson(doc, file);
        file.close();

        if (err) {
            Serial.println("❌ Config JSON parse failed");
            return;
        }

        // Load upload interval
        if (doc.containsKey("upload_interval_ms")) {
            uploadIntervalMs = doc["upload_interval_ms"].as<unsigned long>();
            Serial.printf("✅ Upload interval: %lu ms\n", uploadIntervalMs);
        }

        // Load API endpoint
        if (doc.containsKey("api_endpoint")) {
            apiEndpoint = doc["api_endpoint"].as<String>();
            Serial.printf("✅ API endpoint: %s\n", apiEndpoint.c_str());
        }
    }

    struct UploadParams {
        WebServerModule* self;
        int pm1;
        int pm25;
        int pm10;
        float tvoc;
        float temp;
        float hum;
        int aqi;
    };

    // -----------------------------
    // Cloud Upload
    // -----------------------------
    void sendToVercelAPI(int pm1, int pm25, int pm10,
                         float tvoc, float temp, float hum, int aqi)
    {
        HTTPClient http;
        http.begin(apiEndpoint.c_str());  // Use configurable endpoint
        http.addHeader("Content-Type", "application/json");

        StaticJsonDocument<512> doc;
        doc["pm1_0"] = pm1;
        doc["pm2_5"] = pm25;
        doc["pm10"]  = pm10;
        doc["tvoc"]  = tvoc;
        doc["temperature"] = temp;
        doc["humidity"]    = hum;
        doc["aqi"] = aqi;
        doc["aqi_category"] = IAQ::getAQICategory(aqi);

        String jsonString;
        serializeJson(doc, jsonString);

        int httpCode = http.POST(jsonString);
        Serial.printf("Cloud Upload: %d\n", httpCode);

        http.end();
    }

public:
    WebServerModule(
        AsyncWebServer &srv,
        PMSensor &pm,
        TVOCSensor &tvoc,
        TempHumiditySensor &th,
        bool async = true
    )
    : server(srv),
      pm_sensor(pm),
      tvoc_sensor(tvoc),
      temp_hum_sensor(th),
      asyncPost(async)
    {}

    // -----------------------------
    // Start Web Server (WiFi already connected!)
    // -----------------------------
    void begin(const char* ssid, const char* password) {

        Serial.println("🌐 Starting Web Server...");
        
        // Load configuration
        loadConfig();

        // -------- REST API --------
        server.on("/sensor_data", HTTP_GET, [this](AsyncWebServerRequest *request) {

            PMData pm;
            pm_sensor.read(pm);

            float tvoc = tvoc_sensor.readTVOC();
            float temp = temp_hum_sensor.readTemperature();
            float hum  = temp_hum_sensor.readHumidity();

            int aqi = IAQ::calculateAQI(pm.pm2_5, pm.pm10);
            aqi = IAQ::adjustAQIWithTVOC(aqi, tvoc);

            StaticJsonDocument<512> json;
            json["pm1_0"] = pm.pm1_0;
            json["pm2_5"] = pm.pm2_5;
            json["pm10"]  = pm.pm10;
            json["tvoc"]  = tvoc;
            json["temperature"] = temp;
            json["humidity"]    = hum;
            json["aqi"] = aqi;
            json["aqi_category"] = IAQ::getAQICategory(aqi);

            String out;
            serializeJson(json, out);
            request->send(200, "application/json", out);
        });

        // -------- Dashboard (HTML) --------
        server.serveStatic("/", LittleFS, "/")
              .setDefaultFile("index.html");

        server.begin();
        Serial.println("✅ Web Server Ready!");
    }

    // -----------------------------
    // Cloud Upload Loop
    // -----------------------------
    void loop() {

        if (WiFi.status() != WL_CONNECTED)
            return;

        unsigned long now = millis();
        if (now - lastUploadTime < uploadIntervalMs)
            return;

        lastUploadTime = now;

        PMData pm;
        pm_sensor.read(pm);

        float tvoc = tvoc_sensor.readTVOC();
        float temp = temp_hum_sensor.readTemperature();
        float hum  = temp_hum_sensor.readHumidity();

        int aqi = IAQ::calculateAQI(pm.pm2_5, pm.pm10);
        aqi = IAQ::adjustAQIWithTVOC(aqi, tvoc);

        if (asyncPost) {
            UploadParams* p = new UploadParams{
                this, pm.pm1_0, pm.pm2_5, pm.pm10, tvoc, temp, hum, aqi
            };

            BaseType_t taskCreated = xTaskCreate(
                [](void* data) {
                    UploadParams* p = (UploadParams*)data;
                    p->self->sendToVercelAPI(
                        p->pm1, p->pm25, p->pm10,
                        p->tvoc, p->temp, p->hum, p->aqi
                    );
                    delete p;
                    vTaskDelete(NULL);
                },
                "CloudUploadTask",
                4096,
                p,
                1,
                NULL
            );
            
            // Handle task creation failure
            if (taskCreated != pdPASS) {
                Serial.println("❌ Failed to create upload task - cleaning up");
                delete p;  // Prevent memory leak
            }
        }
        else {
            sendToVercelAPI(pm.pm1_0, pm.pm2_5, pm.pm10, tvoc, temp, hum, aqi);
        }
    }
};
