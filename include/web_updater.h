#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <LittleFS.h>

class WebUpdater {
public:
    static void attach(AsyncWebServer &server) {
        // ----------------------------------------------------
        // GET /update -> Serve update.html
        // ----------------------------------------------------
        server.serveStatic("/update", LittleFS, "/update.html");

        // ----------------------------------------------------
        // POST /update -> Handle File Upload (Actual Endpoint)
        // ----------------------------------------------------
        server.on("/update", HTTP_POST, 
            // Request Handler
            [](AsyncWebServerRequest *request) {
                bool success = !Update.hasError();
                
                const char* htmlSuccess = R"rawliteral(
<!DOCTYPE html><html><head><meta http-equiv="refresh" content="3; url=/"><style>body{background:#121212;color:#fff;font-family:sans-serif;display:flex;align-items:center;justify-content:center;height:100vh;text-align:center;}</style></head>
<body><h1>Update Success! 🚀</h1><p>Rebooting...</p></body></html>
)rawliteral";

                const char* htmlFail = R"rawliteral(
<!DOCTYPE html><html><head><style>body{background:#121212;color:#ef4444;font-family:sans-serif;display:flex;align-items:center;justify-content:center;height:100vh;text-align:center;}</style></head>
<body><h1>Update Failed! ❌</h1><p>Please try again.</p><br><a href="/update" style="color:#fff">Back</a></body></html>
)rawliteral";

                AsyncWebServerResponse *response = request->beginResponse(200, "text/html", success ? htmlSuccess : htmlFail);
                response->addHeader("Connection", "close");
                request->send(response);
                
                if (success) {
                    Serial.println("🔄 Update complete. Rebooting...");
                    delay(100);
                    ESP.restart();
                }
            },
            // Upload Handler (Progress)
            [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
                if (!index) {
                    Serial.printf("📥 Update Start: %s\n", filename.c_str());
                    
                    // If filename includes "spiffs" or "littlefs", update filesystem
                    int cmd = (filename.indexOf("littlefs") > -1 || filename.indexOf("spiffs") > -1) 
                              ? U_SPIFFS : U_FLASH;

                    if (!Update.begin(UPDATE_SIZE_UNKNOWN, cmd)) {
                         Update.printError(Serial);
                    }
                }

                if (!Update.hasError()) {
                    if (Update.write(data, len) != len) {
                        Update.printError(Serial);
                    }
                }

                if (final) {
                    if (Update.end(true)) {
                        Serial.printf("✅ Update Success: %u bytes\n", index + len);
                    } else {
                        Update.printError(Serial);
                    }
                }
            }
        );
    }
};
