#include <Arduino.h>
#include "Config.h"
#include "Acquisition.h"
#include "Buffer.h"
#include "Uploader.h"



Acquisition acquisition;
RingBuffer  ringBuf(BUFFER_CAPACITY);
Uploader    uploader;

uint32_t lastPollMs   = 0;
uint32_t lastUploadMs = 0;

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println(F("EcoWatt M1 Scaffold (Arduino C++)"));
  acquisition.begin();
  uploader.begin();
  lastPollMs = millis();
  lastUploadMs = millis();
}

void loop() {
  const uint32_t now = millis();

  // inverter polling
  if (now - lastPollMs >= POLL_INTERVAL_MS) {
    lastPollMs = now;
    
    Sample s = acquisition.acquire();
    bool ok = ringBuf.push(s);
    if (!ok) {
      Serial.println(F("[WARN] Buffer overwrite occurred."));
    }
    Serial.print(F("[POLL] t=")); Serial.print(s.t_ms);
    Serial.print(F(" v=")); Serial.print(s.v, 2);
    Serial.print(F(" i=")); Serial.println(s.i, 2);
  }

  //15 min upload interval
  if (now - lastUploadMs >= UPLOAD_INTERVAL_MS) {
    lastUploadMs = now;
    
    std::vector<Sample> batch;
    ringBuf.drainTo(batch);  // P2 -> (batch) -> P3
    uploader.upload(batch);  // t_send + t_ack (simulated)
    
  }
}
