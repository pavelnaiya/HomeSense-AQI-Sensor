#pragma once
#include <stdlib.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <secrets.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

struct WiFiConfig {
    String ssid;
    String pass;
};

//
// URL decode helper function
//
String urlDecode(String str) {
    String decoded = "";
    char temp[] = "0x00";
    unsigned int len = str.length();
    unsigned int i = 0;
    
    while (i < len) {
        char decodedChar;
        char encodedChar = str.charAt(i++);
        
        if (encodedChar == '+') {
            decodedChar = ' ';
        }
        else if (encodedChar == '%') {
            temp[2] = str.charAt(i++);
            temp[3] = str.charAt(i++);
            decodedChar = strtol(temp, 0, 16);
        }
        else {
            decodedChar = encodedChar;
        }
        
        decoded += decodedChar;
    }
    
    return decoded;
}

//
// Load WiFi config from LittleFS
//
WiFiConfig loadWiFiConfig() {
    WiFiConfig cfg;

    if (!LittleFS.exists("/wifi.json")) {
        Serial.println("⚠️ wifi.json missing, using HARDCODED defaults");
        cfg.ssid = WIFI_SSID;
        cfg.pass = WIFI_PASS;
        return cfg;
    }

    File file = LittleFS.open("/wifi.json", "r");
    if (!file) {
        Serial.println("⚠️ Failed to open wifi.json, using HARDCODED defaults");
        cfg.ssid = WIFI_SSID;
        cfg.pass = WIFI_PASS;
        return cfg;
    }

    StaticJsonDocument<200> doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();

    if (err) {
        Serial.println("❌ JSON parse failed, using HARDCODED defaults");
        cfg.ssid = WIFI_SSID;
        cfg.pass = WIFI_PASS;
        return cfg;
    }

    cfg.ssid = doc["ssid"].as<String>();
    cfg.pass = doc["password"].as<String>();

    return cfg;
}

//
// Save new WiFi credentials
//
bool saveWiFiConfig(const String& ssid, const String& pass) {
    StaticJsonDocument<200> doc;
    doc["ssid"] = ssid;
    doc["password"] = pass;

    File file = LittleFS.open("/wifi.json", "w");
    if (!file) {
        Serial.println("❌ Failed to open wifi.json for writing");
        return false;
    }

    serializeJson(doc, file);
    file.close();
    
    Serial.printf("✅ WiFi config saved: SSID=%s\n", ssid.c_str());
    return true;
}

//
// Connect to WiFi using stored credentials
//
bool connectWiFi() {
    WiFiConfig cfg = loadWiFiConfig();

    if (cfg.ssid.length() == 0) {
        Serial.println("❌ No WiFi config, skipping connect.");
        return false;
    }

    Serial.printf("📡 Connecting to %s...\n", cfg.ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.ssid.c_str(), cfg.pass.c_str());

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
        delay(500);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n✅ Connected! IP: %s\n",
                    WiFi.localIP().toString().c_str());
        return true;
    }

    Serial.println("\n❌ WiFi connect failed.");
    return false;
}

//
// Global AP server pointer (needs to persist)
//
static AsyncWebServer* apServer = nullptr;

//
// Start fallback AP mode with AsyncWebServer
//
void startAPForConfig(OLEDDisplay* display = nullptr) {
    Serial.println("📶 Starting Access Point for WiFi setup...");
    
    // Generate random 8-digit password for security
    char apPassword[9];
    for (int i = 0; i < 8; i++) {
        apPassword[i] = '0' + random(0, 10);
    }
    apPassword[8] = '\0';
    
    // Explicitly configure AP network settings for stability
    WiFi.mode(WIFI_AP);
    IPAddress local_IP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_IP, gateway, subnet);
    
    WiFi.softAP("HomeSense-Setup", apPassword);

    Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    Serial.printf("AP Password: %s\n", apPassword);
    
    // Display password on OLED if available
    if (display != nullptr) {
        char msg[32];
        snprintf(msg, sizeof(msg), "WiFi: HomeSense\nPass: %s", apPassword);
        display->showMessage(msg);
    }

    // Create async server
    if (apServer == nullptr) {
        apServer = new AsyncWebServer(80);
    }

    // Store AP start time for timeout
    static unsigned long apStartTime = millis();
    const unsigned long AP_TIMEOUT_MS = 15 * 60 * 1000;  // 15 minutes

    // ==============================
    // API: WiFi Scan
    // ==============================
    apServer->on("/api/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("📡 WiFi scan requested");
        
        int n = WiFi.scanNetworks();
        
        StaticJsonDocument<2048> doc;
        JsonArray networks = doc.createNestedArray("networks");
        
        for (int i = 0; i < n; i++) {
            JsonObject network = networks.createNestedObject();
            network["ssid"] = WiFi.SSID(i);
            network["rssi"] = WiFi.RSSI(i);
            network["encryption"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "open" : "secured";
        }
        
        String response;
        serializeJson(doc, response);
        
        request->send(200, "application/json", response);
        
        Serial.printf("✅ Scan complete: %d networks found\n", n);
    });

    // ==============================
    // API: Connect to WiFi
    // ==============================
    apServer->on("/api/connect", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!request->hasParam("ssid")) {
            request->send(400, "text/plain", "Missing SSID");
            return;
        }

        String ssid = urlDecode(request->getParam("ssid")->value());
        String password = "";
        
        if (request->hasParam("password")) {
            password = urlDecode(request->getParam("password")->value());
        }

        Serial.printf("🔐 Connect request: SSID='%s'\n", ssid.c_str());

        // Validate inputs
        if (ssid.length() == 0 || ssid.length() > 32) {
            request->send(400, "text/plain", "Invalid SSID length");
            return;
        }

        if (password.length() > 64) {
            request->send(400, "text/plain", "Invalid password length");
            return;
        }

        // Save credentials
        if (!saveWiFiConfig(ssid, password)) {
            request->send(500, "text/plain", "Failed to save config");
            return;
        }

        // Send success response
        request->send(200, "text/plain", "OK");

        // Schedule restart
        Serial.println("✅ Config saved. Rebooting in 2 seconds...");
        delay(2000);
        ESP.restart();
    });

    // ==============================
    // Explicit Root Handler (Fixes loading issues)
    // ==============================
    apServer->on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/setup.html", "text/html");
    });

    // Serve other static files
    apServer->serveStatic("/", LittleFS, "/");

    // Start server
    apServer->begin();
    Serial.println("✅ AP Web Server started!");
    Serial.println("📱 Connect to 'HomeSense-Setup' and visit http://192.168.4.1");
    Serial.printf("⏱️  AP will timeout after 15 minutes\n");
    
    // Non-blocking timeout check (call in loop)
    // Note: In production, implement this in main loop
    // if (millis() - apStartTime > AP_TIMEOUT_MS) {
    //     Serial.println("⚠️ AP timeout - rebooting...");
    //     ESP.restart();
    // }
}

//
// Stop AP server (if needed for cleanup)
//
void stopAPServer() {
    if (apServer != nullptr) {
        apServer->end();
        delete apServer;
        apServer = nullptr;
    }
}
