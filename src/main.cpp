#include <Arduino.h>
#include "Config.h"
#include "Acquisition.h"
#include "Buffer.h"
#include "Uploader.h"

// Petri Net mapping (places):
// P0 Idle, P1 ReadyToPoll, P2 Buffering, P3 UploadWindow, P4 Ack
// Transitions: t_poll, t_buf, t_tick15, t_send, t_ack

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

  // --- t_poll: periodic acquisition ---
  if (now - lastPollMs >= POLL_INTERVAL_MS) {
    lastPollMs = now;
    // P1 -> P2 via t_poll/t_buf
    Sample s = acquisition.acquire();
    bool ok = ringBuf.push(s);
    if (!ok) {
      Serial.println(F("[WARN] Buffer overwrite occurred."));
    }
    Serial.print(F("[POLL] t=")); Serial.print(s.t_ms);
    Serial.print(F(" v=")); Serial.print(s.v, 2);
    Serial.print(F(" i=")); Serial.println(s.i, 2);
  }

  // --- t_tick15: periodic upload window ---
  if (now - lastUploadMs >= UPLOAD_INTERVAL_MS) {
    lastUploadMs = now;
    // P3 UploadWindow
    std::vector<Sample> batch;
    ringBuf.drainTo(batch);  // P2 -> (batch) -> P3
    uploader.upload(batch);  // t_send + t_ack (simulated)
    // P4 Ack -> P0 Idle (back to normal loop)
  }
}
