#pragma once
#include <Arduino.h>
#include <HardwareSerial.h>

// -----------------------------------
// PM Data Structure
// -----------------------------------
struct PMData {
    uint16_t pm1_0;
    uint16_t pm2_5;
    uint16_t pm10;
};

// -----------------------------------
// PM Sensor Class (Winsen ZH07)
// -----------------------------------
class PMSensor {
private:
    HardwareSerial &serial;

public:
    // Constructor expects a HardwareSerial object (e.g., Serial2)
    PMSensor(HardwareSerial &ser) : serial(ser) {}

    // Begin UART communication with ZH07
    void begin(int rxPin, int txPin, uint32_t baud = 9600) {
        // ✅ Correct: use rxPin & txPin provided from user
        serial.begin(baud, SERIAL_8N1, rxPin, txPin);
        delay(300);

        // Clear junk bytes
        while (serial.available()) serial.read();
    }

    // Read PM values into the PMData struct
    bool read(PMData &data) {
        if (!serial.available()) return false;

        // Wait for header 0x42 0x4D
        while (serial.available()) {
            if (serial.read() == 0x42) {
                if (serial.peek() == 0x4D) {
                    serial.read(); // consume 0x4D
                    break;
                }
            }
        }

        // Need 30 more bytes
        unsigned long start = millis();
        while (serial.available() < 30 && millis() - start < 1000) {
            delay(5);
        }

        if (serial.available() < 30) return false;

        uint8_t buffer[30];
        serial.readBytes(buffer, 30);

        // Validate checksum
        uint16_t sum = 0x42 + 0x4D;
        for (int i = 0; i < 28; i++) sum += buffer[i];

        uint16_t checksum = (buffer[28] << 8) | buffer[29];

        if (checksum != sum) return false;

        // Extract values
        data.pm1_0 = (buffer[4] << 8) | buffer[5];
        data.pm2_5 = (buffer[6] << 8) | buffer[7];
        data.pm10  = (buffer[8] << 8) | buffer[9];

        return true;
    }
};
