#pragma once
#include <Arduino.h>
#include "Config.h"

class Acquisition {
public:
    void begin() {
    #if defined(ARDUINO_ARCH_ESP8266)
      randomSeed(ESP.getChipId());             // ESP8266
    #elif defined(ARDUINO_ARCH_ESP32)
      uint64_t mac = ESP.getEfuseMac();        // ESP32
      uint32_t seed = (uint32_t)(mac ^ (mac >> 32) ^ millis());
      randomSeed(seed);
    #else
      randomSeed((uint32_t)millis());          // PC/native or others
    #endif
}


  // Simulate a single read from the inverter (replace with real call later)
  Sample acquire() {
    Sample s;
    s.t_ms = millis();
    // Fake but stable-ish signals: voltage around 230±5, current 5±0.5
    float phase = (millis() % 60000) / 60000.0f * 2.0f * PI;
    s.v = 230.0f + 5.0f * sinf(phase) + (random(-50, 50) / 100.0f);
    s.i = 5.0f   + 0.5f * cosf(phase) + (random(-20, 20) / 100.0f);
    return s;
  }
};
