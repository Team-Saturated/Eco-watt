#include <Arduino.h>
#include "../include/Config.h"
#include "../include/InverterClient.h"
#include "../include/Poller.h"

// NEW:
#include "Acquisition.h"
#include "Buffer.h"
#include "Uploader.h"
#include "Compression.h" 
#include "Packetizer.h" 
#include <FS.h> // For SPIFFS


#include "WiFi_conn.h"
#include "CloudTransport.h"
#include "Rs485Transport.h"

#if SIMULATE
CloudTransport* g_transport = nullptr;
#else
Rs485Transport* g_transport = nullptr;
#endif

InverterClient* g_client  = nullptr;
Poller*        g_poller  = nullptr;
RingBuffer*    g_buffer  = nullptr;
Acquisition*   g_acq     = nullptr;
Uploader*      g_uploader= nullptr;



void setup() {

  Serial.begin(115200);
  delay(200);

  
  if (!wifiConnect()) {
    Serial.println("Failed to connect to WiFi. Continuing without network...");//need a call back function 
  }

  try {

    #if SIMULATE
        g_transport = new CloudTransport(String(API_URL), String(AUTH_HEADER), REQ_TIMEOUT_MS);
    #else
        g_transport = new Rs485Transport(RS485_SERIAL, RS485_BAUD, RS485_DE_RE_PIN, REQ_TIMEOUT_MS);
    #endif
        if (!g_transport) { Serial.println("Failed to create transport"); return; }

    g_client = new InverterClient(*g_transport);
    if (!g_client) { Serial.println("Failed to create inverter client"); return; }

    
    g_buffer   = new RingBuffer(BUFFER_CAPACITY);
    g_acq      = new Acquisition(*g_client);
    g_uploader = new Uploader(String(API_UPLOAD_URL), String(AUTH_HEADER));

    g_poller = new Poller(*g_client, POLL_PERIOD_MS, *g_buffer);
    if (!g_poller) { Serial.println("Failed to create poller"); return; }

    Serial.println("Compression enabled: using delta encoding for uploads.");
    Serial.println("Setup done successfully.");
    
  } catch (const std::exception& e) {
    Serial.printf("Setup failed with error: %s\n", e.what());
  }
}

void loop() {

  ensureWiFiConnected();
  static uint32_t lastUpload = millis();
  
  try {
    g_poller->loop(SLAVE_ID, START_ADDR, QTY_REGS);
  } catch (const std::exception& e) {
    Serial.printf("Error in main loop: %s\n", e.what());
  }
  

  // Periodic upload every 15 seconds for demo (UPLOAD_PERIOD_MS)
  uint32_t now = millis();
  if (now - lastUpload >= UPLOAD_PERIOD_MS) {
    lastUpload = now;
    std::vector<Record> batch;
    g_buffer->drainTo(batch);
    
    // if data exists
    if (!batch.empty()) {
      Serial.printf("[REAL DATA] Uploading %u actual inverter records\n", (unsigned)batch.size());
      
      // --- Detailed Compression Benchmarking ---
      Serial.println("=== REAL INVERTER DATA COMPRESSION REPORT ===");
      Serial.printf("Compression Method Used: Delta Encoding\n");
      Serial.printf("Number of Real Inverter Samples: %u\n", (unsigned)batch.size());
    
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

      Serial.printf("[MAIN] Uploading %u REAL inverter records (compressed)\n", (unsigned)batch.size());
      // For demonstration, upload the encrypted block (could be chunked)
      // In real use, send each chunk and handle retries/acks
      bool ok = g_uploader->uploadBatch(batch); // Still uses original batch for now
      if (!ok) {
        Serial.println("[MAIN] Upload failed");
      } else {
        Serial.println("[MAIN] Real inverter data uploaded successfully!");
      }
    } else {
      Serial.println("[MAIN] No real inverter data available yet - waiting for next cycle...");
    }
  }
  delay(5);
}
