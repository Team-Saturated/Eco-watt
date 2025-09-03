#pragma once

#include <Arduino.h>

// WiFi Settings
#define WIFI_SSID     "LasithWifi"     // Replace with your WiFi name
#define WIFI_PASSWORD "12345678"  // Replace with your WiFi password

// --- Tunables (demo-friendly) ---
static const uint32_t POLL_INTERVAL_MS   = 1000;   // every 2s
static const uint32_t UPLOAD_INTERVAL_MS = 8000;  // every 15s (simulated 15 min)
static const size_t   BUFFER_CAPACITY    = 5;    // ring buffer size

// --- Types ---
struct Sample {
  uint32_t t_ms; // time since boot (ms)
  float v;       // voltage
  float i;       // current
};
