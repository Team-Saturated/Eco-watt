#include "Poller.h"
#include "Config.h"
#include "Acquisition.h"
#include "Buffer.h"
#include "Uploader.h"

// Extern globals created in main.cpp
extern RingBuffer* g_buffer;
extern Acquisition* g_acq;
extern Uploader* g_uploader;
static uint32_t g_lastUpload = 0;

void Poller::loop(uint8_t slave, uint16_t addr, uint16_t qty) {
  uint32_t now = millis();
  if (now < _next) return;

  // === Acquire one sample ===
  Record rec;
  String err;
  if (g_acq && g_acq->acquire(slave, addr, qty, rec, err)) {
#if SIMULATE
    Serial.printf("[CLOUD OK] %s\n", rec.rawFrameHex.c_str());
#else
    Serial.print("[RS485 OK] ");
    for (const auto& r : rec.regs) {
      Serial.printf("Addr=%u -> %.3f %s | ", r.addr, r.value, r.unit.c_str());
    }
    Serial.println();
#endif
    if (g_buffer) g_buffer->push(rec);
  } else {
    Serial.printf("[ERR] acquire: %s\n", err.c_str());
  }

  // // === Upload window ===
  // if (g_uploader && (now - g_lastUpload >= UPLOAD_PERIOD_MS)) {
  //   g_lastUpload = now;
  //   std::vector<Record> batch;
  //   if (g_buffer) g_buffer->drainTo(batch);

  //   if (!batch.empty()) {
  //     bool ok = g_uploader->uploadBatch(batch);
  //     Serial.printf("[UPLOAD] ok=%d http=%d sent=%u\n",
  //                   ok ? 1 : 0,
  //                   g_uploader->last_http_status(),
  //                   (unsigned)batch.size());
  //     // Optionally: if upload failed, you could merge batch back into g_buffer
  //   }
  // }
   // === Upload window ===
   
  if (now - g_lastUpload >= UPLOAD_PERIOD_MS) {
    g_lastUpload = now;
    std::vector<Record> batch;
    if (g_buffer) g_buffer->drainTo(batch);

    if (!batch.empty()) {
      // Instead of g_uploader->uploadBatch(batch) ...
      Serial.printf("[BATCH] %u records:\n", (unsigned)batch.size());
      for (const auto& rec : batch) {
        Serial.printf("  t=%llu start=%u qty=%u\n",
                      rec.ts_ms, rec.start, rec.qty);
        for (const auto& r : rec.regs) {
          Serial.printf("    Addr=%u Raw=0x%04X -> %.3f %s\n",
                        r.addr, r.raw, r.value, r.unit.c_str());
        }
      }
    }
  }

  _next = now + _period;
}
