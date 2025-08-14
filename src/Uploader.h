#pragma once
#include <Arduino.h>
#include <vector>
#include "Config.h"

class Uploader {
public:
  void begin() {
    // Initialize WiFi/HTTP/MQTT later.
  }

  // Simulated upload: print a compact JSON-like block to Serial.
  void upload(const std::vector<Sample>& batch) {
    Serial.println(F("=== [UPLOAD WINDOW OPEN] ==="));
    Serial.print(F("{\"count\":")); Serial.print(batch.size()); Serial.print(F(",\"samples\":["));
    for (size_t i = 0; i < batch.size(); ++i) {
      const auto& s = batch[i];
      Serial.print(F("{\"t\":")); Serial.print(s.t_ms);
      Serial.print(F(",\"v\":")); Serial.print(s.v, 2);
      Serial.print(F(",\"i\":")); Serial.print(s.i, 2);
      Serial.print(F("}"));
      if (i + 1 < batch.size()) Serial.print(F(","));
    }
    Serial.println(F("]}"));
    Serial.println(F("=== [UPLOAD ACK RECEIVED] ==="));
  }
};
