#pragma once

#include <Arduino.h>

// --- Tunables (demo-friendly) ---
static const uint32_t POLL_INTERVAL_MS   = 1000;   // every 1s
static const uint32_t UPLOAD_INTERVAL_MS = 8000;  // every 8s 
static const size_t   BUFFER_CAPACITY    = 5;    // ring buffer size

// --- Types ---
struct Sample {
  uint32_t t_ms; // time since boot (ms)
  float v;       // voltage
  float i;       // current
};
