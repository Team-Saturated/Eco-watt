#pragma once

#include <Arduino.h>

// --- Tunables (demo-friendly) ---
static const uint32_t POLL_INTERVAL_MS   = 2000;   // every 2s
static const uint32_t UPLOAD_INTERVAL_MS = 15000;  // every 15s (simulated 15 min)
static const size_t   BUFFER_CAPACITY    = 256;    // ring buffer size

// --- Types ---
struct Sample {
  uint32_t t_ms; // time since boot (ms)
  float v;       // voltage
  float i;       // current
};
