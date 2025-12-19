#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <ArduinoJson.h>

/**
 * WebUpdater: A reusable GitHub-based OTA updater.
 */
class WebUpdater {
public:
    // --- Project Configuration ---
    static constexpr const char* GH_USER = "pavelnaiya";
    static constexpr const char* GH_REPO = "HomeSense-AQI-Sensor";
    static constexpr const char* GH_BIN  = "firmware.ino.bin"; 
    static constexpr const char* VERSION = "1.0.1";
    // ----------------------------

    static void checkAndApplyUpdate() {
        if (WiFi.status() != WL_CONNECTED) return;

        // Construct version URL (Standard GitHub Raw format)
        String versionUrl = String("https://raw.githubusercontent.com/") + GH_USER + "/" + GH_REPO + "/main/version.json";

        Serial.println("🔍 Checking GitHub for updates...");
        
        WiFiClientSecure client;
        client.setInsecure(); 

        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        
        if (http.begin(client, versionUrl)) {
            int httpCode = http.GET();
            if (httpCode == HTTP_CODE_OK) {
                String payload = http.getString();
                StaticJsonDocument<512> doc;
                DeserializationError error = deserializeJson(doc, payload);

                if (error) {
                    Serial.println("❌ JSON Parse Failed");
                    http.end();
                    return;
                }

                const char* latestVersion = doc["version"] | "";
                const char* description = doc["description"] | "No description.";

                Serial.printf("📡 Version: Device [%s] | GitHub [%s]\n", VERSION, latestVersion);

                if (String(latestVersion) != VERSION && String(latestVersion).length() > 0) {
                    Serial.println("🚀 New version found!");
                    Serial.printf("📝 Changes: %s\n", description);
                    
                    // Construct binary URL using the new version tag (e.g., v1.0.1)
                    String tag = String("v") + latestVersion;
                    String firmwareUrl = String("https://github.com/") + GH_USER + "/" + GH_REPO + "/releases/download/" + tag + "/" + GH_BIN;
                    
                    performGitHubUpdate(client, firmwareUrl);
                } else {
                    Serial.println("✅ Firmware is already latest.");
                }
            } else {
                Serial.printf("❌ Failed to fetch version.json (HTTP %d)\n", httpCode);
            }
            http.end();
        }
    }

private:
    static void performGitHubUpdate(WiFiClientSecure &client, String url) {
        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        
        Serial.println("📥 Downloading firmware binary...");
        if (http.begin(client, url)) {
            int httpCode = http.GET();

            if (httpCode == HTTP_CODE_OK) {
                int contentLength = http.getSize();
                if (contentLength > 0) {
                    Serial.printf("📦 Size: %d bytes. Flashing...\n", contentLength);
                    
                    if (Update.begin(contentLength)) {
                        size_t written = Update.writeStream(*http.getStreamPtr());
                        if (written == contentLength) {
                             if (Update.end()) {
                                Serial.println("🏁 Update SUCCESS! Rebooting...");
                                delay(2000);
                                ESP.restart();
                            } else {
                                Serial.printf("❌ Flash End Error: %s\n", Update.errorString());
                            }
                        } else {
                            Serial.printf("❌ Write Error: Only %d/%d bytes written\n", written, contentLength);
                        }
                    } else {
                        Serial.println("❌ Update Begin Error: Not enough space");
                    }
                } else {
                    Serial.println("❌ Invalid firmware size");
                }
            } else {
                Serial.printf("❌ Download Failed (HTTP %d)\n", httpCode);
            }
            http.end();
        } else {
            Serial.println("❌ Failed to connect for download");
        }
    }
};
