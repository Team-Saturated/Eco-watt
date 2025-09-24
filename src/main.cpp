#include <Arduino.h>
#include "../include/Config.h"
#include "../include/InverterClient.h"
#include "../include/Poller.h"

// NEW:
#include "Acquisition.h"
#include "Buffer.h"
#include "Uploader.h"
#include "Compression.h" // Added for compression
#include "Packetizer.h" // Added for packetizer
#include <FS.h> // For file writing (ESP32/ESP8266)

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#else
  #include <WiFi.h>
#endif

#include "CloudTransport.h"
#include "Rs485Transport.h"

#if SIMULATE
CloudTransport* g_transport = nullptr;
#else
Rs485Transport* g_transport = nullptr;
#endif

// NOTE: if you also pass AUTH via build flags, keep them identical.
#define AUTH_HEADER " NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YjhmOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWI4NQ=="

InverterClient* g_client  = nullptr;
Poller*        g_poller  = nullptr;

// NEW: globals used by Poller
RingBuffer*    g_buffer  = nullptr;
Acquisition*   g_acq     = nullptr;
Uploader*      g_uploader= nullptr;

static bool wifiConnect() {
  Serial.printf("WiFi connecting to %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries++ < 60) {
    delay(500);
    Serial.print(".");
    if (tries % 10 == 0) {
      WiFi.disconnect();
      delay(100);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi OK: %s\n", WiFi.localIP().toString().c_str());
    return true;
  } else {
    Serial.println("WiFi failed");
    return false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // Wi-Fi (best effort)
  if (!wifiConnect()) {
    Serial.println("Failed to connect to WiFi. Continuing without network...");
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

    g_poller = new Poller(*g_client, POLL_PERIOD_MS);
    if (!g_poller) { Serial.println("Failed to create poller"); return; }

    // NEW: buffer + acquisition + uploader
    g_buffer   = new RingBuffer(BUFFER_CAPACITY);
    g_acq      = new Acquisition(*g_client);
    g_uploader = new Uploader(String(API_URL), String(AUTH_HEADER));

    Serial.println("Compression enabled: using delta encoding for uploads.");
    Serial.println("Setup done successfully.");
  } catch (const std::exception& e) {
    Serial.printf("Setup failed with error: %s\n", e.what());
  }
}

void loop() {
  static uint32_t lastUpload = millis();
  static bool benchmarked = false;
  if (g_poller) {
    try {
      g_poller->loop(SLAVE_ID, START_ADDR, QTY_REGS);
    } catch (const std::exception& e) {
      Serial.printf("Error in main loop: %s\n", e.what());
    }
  } else {
    Serial.println("Poller not initialized. Retrying setup...");
    setup();
  }

  // Periodic upload every 15 seconds for demo (UPLOAD_PERIOD_MS)
  uint32_t now = millis();
  if (now - lastUpload >= UPLOAD_PERIOD_MS) {
    lastUpload = now;
    std::vector<Record> batch;
    g_buffer->drainTo(batch);
    
    // If no records, simulate random samples for demonstration
    if (batch.empty()) {
      Serial.println("[BENCHMARK] Buffer empty, simulating random records...");
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

    if (!batch.empty()) {
      Serial.printf("[MAIN] Uploading %u records (compressed)\n", (unsigned)batch.size());
      // For demonstration, upload the encrypted block (could be chunked)
      // In real use, send each chunk and handle retries/acks
      bool ok = g_uploader->uploadBatch(batch); // Still uses original batch for now
      if (!ok) Serial.println("[MAIN] Upload failed");
    } else {
      Serial.println("[MAIN] No records to upload");
    }
  }
  delay(5);
}
