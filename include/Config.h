#pragma once
#include <Arduino.h>

// ---------- WiFi ----------
#define WIFI_SSID     "LasithWifi"     // Replace with your WiFi name
#define WIFI_PASSWORD "12345678"       // Replace with your WiFi password

// ---------- Polling & request ----------
extern uint16_t POLL_PERIOD_MS;           // how often we poll the inverter
#define REQ_TIMEOUT_MS  5000           // HTTP/RS485 request timeout

// ---------- RS-485 (ignored when SIMULATE=1) ----------
#define RS485_SERIAL     Serial2       // ESP32; for ESP8266 use Serial
#define RS485_BAUD       9600
#define RS485_DE_RE_PIN  21            // change to your DE/RE pin
#define SLAVE_ID         0x11
#define START_ADDR       0x0000
#define QTY_REGS         10

// ---------- Buffering & Upload schedule ----------
extern uint16_t UPLOAD_PERIOD_MS;      // send buffered data every 14 sec (before Poller flush at 15s)
extern uint16_t BUFFER_CAPACITY;
extern uint16_t REG_REQ_ID_1;        // number of samples to keep in RAM
#define MAX_BATCH_BYTES    8192        // cap payload size per upload (approx)

// ---------- Batch Collection Parameters ----------
#define TARGET_BATCH_SIZE  10          // collect exactly 10 records before upload
#define MAX_BATCH_TIME_MS  30000       // max wait time (30s) - upload whatever we have

// Retry policy for failed uploads
#define UPLOAD_MAX_RETRIES  2          // additional tries within a window
#define RETRY_DELAY_MS      2000       // delay between retries

// ---------- Upload mode ----------
// Choose what we send to the cloud: decoded register values or raw frames
#define UPLOAD_MODE_DECODED  1
#define UPLOAD_MODE_RAW      2
#define UPLOAD_MODE          UPLOAD_MODE_DECODED  // default

// Error/backoff policy
#define BACKOFF_MIN_MS   2000     // first backoff after an error
#define BACKOFF_STEP_MS  2000     // add per consecutive error
#define BACKOFF_MAX_MS   30000    // cap backoff
#define ERR_RESET_AFTER  1        // reset backoff after this many successes

//Mqtt
extern const char* MQTT_HOST; // or cloud host
extern const uint16_t MQTT_PORT;
#define CONNECTION_CHECK_PERIOD_MS  2000   // how often we check connection status

#define WRITE 1
#define READ 0
extern uint16_t WRITE_ADDR;
extern uint16_t WRITE_VALUE;