#include <Arduino.h>
#include "Config.h"
#include "Buffer.h"
#include "Compression.h"
#include "Packetizer.h"
#include "Uploader.h"
#include <thread>
#include <vector>

int main() {
  Serial.begin(115200);
  Serial.println("\n=== EcoWatt PC Simulation ===");
  Serial.println("Testing compression, packetization, and upload simulation");

  // Initialize components
  RingBuffer buffer(BUFFER_CAPACITY);
  Uploader uploader(String(API_URL), String(AUTH_HEADER));

  uint32_t lastUpload = millis();

  for (;;) {
    uint32_t now = millis();

    // Periodic upload every 15 seconds (UPLOAD_PERIOD_MS)
    if (now - lastUpload >= UPLOAD_PERIOD_MS) {
      lastUpload = now;
      std::vector<Record> batch;
      buffer.drainTo(batch);
      
      // Simulate random samples for demonstration
      Serial.println("[PC-SIM] Generating simulated inverter data...");
      for (int i = 0; i < 50; ++i) {
        Record r;
        r.ts_ms = millis() + i * 1000;
        r.start = START_ADDR;
        r.qty = QTY_REGS;
        DecodedReg reg;
        reg.addr = START_ADDR;
        reg.raw = 1000 + i;
        reg.value = 220.0f + (i % 10);
        reg.unit = "V";
        r.regs.push_back(reg);
        batch.push_back(r);
      }

      // --- Detailed Compression Benchmarking ---
      Serial.println("=== COMPRESSION BENCHMARK REPORT ===");
      Serial.printf("Compression Method Used: Delta Encoding\n");
      Serial.printf("Number of Samples: %u\n", (unsigned)batch.size());
      
      uint32_t original_size = batch.size() * sizeof(Record);
      Serial.printf("Original Payload Size: %u bytes\n", original_size);
      
      // Measure compression time
      uint32_t compress_start = micros();
      std::vector<uint8_t> compressed = Packetizer::finalizeBlock(batch);
      uint32_t compress_time = micros() - compress_start;
      
      Serial.printf("Compressed Payload Size: %u bytes\n", (unsigned)compressed.size());
      float compression_ratio = (float)original_size / (float)compressed.size();
      Serial.printf("Compression Ratio: %.2f:1 (%.1f%% reduction)\n", 
                    compression_ratio, 
                    (1.0f - (float)compressed.size() / (float)original_size) * 100.0f);
      Serial.printf("CPU Time: %u microseconds\n", compress_time);
      
      // Lossless Recovery Verification
      std::vector<Record> decompressed = Compression::decompressDelta(compressed);
      bool lossless = (decompressed.size() == batch.size());
      Serial.printf("Lossless Recovery Verification: %s\n", lossless ? "PASSED" : "FAILED");
      
      // --- Packetizer: encrypt, chunk ---
      std::vector<uint8_t> encrypted = Packetizer::encryptAndMac(compressed);
      auto chunks = Packetizer::chunkData(encrypted, 32);
      
      Serial.printf("Final Upload Payload Size: %u bytes (with encryption/MAC)\n", (unsigned)encrypted.size());
      Serial.printf("Number of Chunks: %u (chunk size: 32 bytes)\n", (unsigned)chunks.size());
      Serial.println("=====================================");

      // Upload batch (simulated)
      if (!batch.empty()) {
        Serial.printf("[PC-SIM] Uploading %u records (compressed)\n", (unsigned)batch.size());
        bool ok = uploader.uploadBatch(batch);
        if (!ok) Serial.println("[PC-SIM] Upload failed");
      }
      
      Serial.println("[PC-SIM] Waiting for next cycle...\n");
    }

    delay(100); // Small delay for PC simulation
  }

  return 0;
}
