#pragma once

//-----------------------------------------------------------------------------
// WiFi Configuration
//-----------------------------------------------------------------------------
#define WIFI_SSID     "LasithWifi"     // Your WiFi network name
#define WIFI_PASSWORD "12345678"        // Your WiFi password

//-----------------------------------------------------------------------------
// API Configuration
//-----------------------------------------------------------------------------
#define API_URL      "http://20.15.114.131:8080/api/inverter/read"  // API endpoint
#define AUTH_HEADER  "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkVjb3dhdHQgRGV2aWNlIiwiaWF0IjoxNTE2MjM5MDIyfQ.SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c"

//-----------------------------------------------------------------------------
// Timing Configuration
//-----------------------------------------------------------------------------
#define POLL_PERIOD_MS   1000    // How often to poll the inverter
#define REQ_TIMEOUT_MS   5000    // HTTP request timeout
#define WIFI_TIMEOUT_MS  30000   // WiFi connection timeout

//-----------------------------------------------------------------------------
// RS-485 Configuration (ignored in simulation)
//-----------------------------------------------------------------------------
#define RS485_SERIAL    Serial2     // ESP32; for ESP8266 use Serial
#define RS485_BAUD      9600        // Modbus baud rate
#define RS485_DE_RE_PIN 21          // DE/RE pin for RS485 communication

//-----------------------------------------------------------------------------
// Modbus Configuration
//-----------------------------------------------------------------------------
#define SLAVE_ID        0x11        // Modbus slave address
#define START_ADDR      0x0000      // Starting register address
#define QTY_REGS        2           // Number of registers to read

//-----------------------------------------------------------------------------
// Feature Flags
//-----------------------------------------------------------------------------
#ifndef SIMULATE
#define SIMULATE 1                   // 1 for cloud simulation, 0 for RS485
#endif
