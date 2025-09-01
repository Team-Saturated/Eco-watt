#pragma once

#define WIFI_SSID     "SLT-Fiber-TsS7z-2.4G"
#define WIFI_PASSWORD "ktGHS269"

#define POLL_PERIOD_MS 1000
#define REQ_TIMEOUT_MS 5000

// RS-485 only (ignored in simulation)
#define RS485_SERIAL    Serial2     // ESP32; for ESP8266 use Serial
#define RS485_BAUD      9600
#define RS485_DE_RE_PIN 21          // change to your DE/RE pin
#define SLAVE_ID        0x11
#define START_ADDR      0x0000
#define QTY_REGS        2
