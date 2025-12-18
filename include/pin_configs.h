#pragma once

// -----------------------
// PM Sensor (Winsen ZH07)
// -----------------------
#define PM_RX_PIN 32
#define PM_TX_PIN 33

// -----------------------
// AGS02MA TVOC Sensor
// -----------------------
#define AGS_SDA_PIN 18
#define AGS_SCL_PIN 19

// -----------------------
// DHT11 Sensor
// -----------------------
#define DHT_PIN 25
#define DHT_TYPE DHT11

// -----------------------
// OLED Display (I2C)
// -----------------------

#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_ADDR 0x3C 

// -----------------------
// Touch Sensor (TTP223B)
// -----------------------
#define TOUCH_PIN 4      // Active-low digital input

// -----------------------
// Battery Sensing (Voltage Divider: 100k + 100k)
// -----------------------
#define BATTERY_PIN 34
#define VOLT_DIVIDER_RATIO 2.0  // Ratio to multiply ADC voltage by
