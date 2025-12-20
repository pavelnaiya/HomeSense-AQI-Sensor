#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "oled_display.h"
#include "config.h"

/**
 * WebUpdater: A reusable GitHub-based OTA updater.
 */
class WebUpdater {
public:
    // --- Project Configuration (from config.h) ---
    static constexpr const char* GH_USER = GITHUB_USER;
    static constexpr const char* GH_REPO = GITHUB_REPO;
    static constexpr const char* GH_BIN  = GITHUB_BIN_FILENAME; 
    static constexpr const char* VERSION = FIRMWARE_VERSION;
    // ----------------------------

    static void checkAndApplyUpdate(OLEDDisplay* display = nullptr) {
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
                    
                    // Show update animation on display
                    if (display) {
                        display->showUpdateAnimation(latestVersion, -1);
                    }
                    
                    // Construct binary URL using the new version tag (e.g., v1.0.1)
                    String tag = String("v") + latestVersion;
                    String firmwareUrl = String("https://github.com/") + GH_USER + "/" + GH_REPO + "/releases/download/" + tag + "/" + GH_BIN;
                    
                    performGitHubUpdate(client, firmwareUrl, display);
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
    static void performGitHubUpdate(WiFiClientSecure &client, String url, OLEDDisplay* display = nullptr) {
        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        
        Serial.println("📥 Downloading firmware binary...");
        if (display) {
            display->showUpdateAnimation(nullptr, 0); // Show 0% progress
        }
        
        if (http.begin(client, url)) {
            int httpCode = http.GET();

            if (httpCode == HTTP_CODE_OK) {
                int contentLength = http.getSize();
                if (contentLength > 0) {
                    Serial.printf("📦 Size: %d bytes. Flashing...\n", contentLength);
                    
                    // Set partition to OTA_0 (app partition for OTA updates)
                    if (Update.begin(contentLength, U_FLASH)) {
                        Serial.println("📝 Starting firmware flash...");
                        
                        // Stream with progress updates
                        WiFiClient* stream = http.getStreamPtr();
                        size_t written = 0;
                        const size_t bufferSize = 512;
                        uint8_t buffer[bufferSize];
                        
                        while (http.connected() && (written < contentLength)) {
                            size_t available = stream->available();
                            if (available) {
                                size_t toRead = (available > bufferSize) ? bufferSize : available;
                                size_t read = stream->readBytes(buffer, toRead);
                                size_t writtenThisChunk = Update.write(buffer, read);
                                
                                if (writtenThisChunk != read) {
                                    Serial.printf("⚠️ Write mismatch: read %d, wrote %d\n", read, writtenThisChunk);
                                }
                                
                                written += writtenThisChunk;
                                
                                // Update progress on display
                                if (display && contentLength > 0) {
                                    int progress = (written * 100) / contentLength;
                                    display->showUpdateAnimation(nullptr, progress);
                                }
                                
                                // Periodic progress to Serial
                                if (written % 10000 == 0 || written == contentLength) {
                                    Serial.printf("📊 Progress: %d/%d bytes (%.1f%%)\n", 
                                                  written, contentLength, (written * 100.0f) / contentLength);
                                }
                            }
                            delay(1);
                        }
                        
                        if (written == contentLength) {
                            Serial.println("✅ All bytes written, finalizing update...");
                            if (display) {
                                display->showUpdateAnimation(nullptr, 100); // Show 100%
                                delay(500);
                            }
                            
                            // Commit the update (true = even if validation fails, commit it)
                            if (Update.end(true)) {
                                Serial.println("🏁 Update SUCCESS! Firmware committed.");
                                Serial.println("🔄 Rebooting in 2 seconds...");
                                if (display) {
                                    display->showMessage("Update OK!\nRebooting...");
                                }
                                delay(2000);
                                ESP.restart();
                            } else {
                                Serial.printf("❌ Flash End Error: %s\n", Update.errorString());
                                Serial.printf("❌ Error code: %u\n", Update.getError());
                                if (display) {
                                    display->showMessage("Commit Failed!");
                                }
                                Update.abort();
                            }
                        } else {
                            Serial.printf("❌ Write Error: Only %d/%d bytes written\n", written, contentLength);
                            if (display) {
                                display->showMessage("Write Error!");
                            }
                            Update.abort();
                        }
                    } else {
                        Serial.println("❌ Update Begin Error: Not enough space");
                        if (display) {
                            display->showMessage("No Space!");
                        }
                    }
                } else {
                    Serial.println("❌ Invalid firmware size");
                    if (display) {
                        display->showMessage("Invalid Size!");
                    }
                }
            } else {
                Serial.printf("❌ Download Failed (HTTP %d)\n", httpCode);
                if (display) {
                    display->showMessage("Download Failed!");
                }
            }
            http.end();
        } else {
            Serial.println("❌ Failed to connect for download");
            if (display) {
                display->showMessage("Connect Failed!");
            }
        }
    }
};
