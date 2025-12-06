#pragma once
#include <DHT.h>
#include "pin_configs.h"

class TempHumiditySensor {
private:
    DHT dht;

public:
    // Correct constructor: use the provided pin & type
    TempHumiditySensor(uint8_t pin, uint8_t type)
        : dht(pin, type) {}

    void begin() {
        dht.begin();
    }

    // Read temperature in Celsius
    float readTemperature() {
        float temp = dht.readTemperature();
        return isnan(temp) ? NAN : temp;
    }

    // Read humidity in %
    float readHumidity() {
        float hum = dht.readHumidity();
        return isnan(hum) ? NAN : hum;
    }
};
