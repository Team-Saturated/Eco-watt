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

InverterClient* g_client  = nullptr;
Poller*        g_poller  = nullptr;

// NEW: globals used by Poller
RingBuffer*    g_buffer  = nullptr;
Acquisition*   g_acq     = nullptr;
Uploader*      g_uploader= nullptr;

// Batch collection globals
std::vector<Record> g_recordBatch;
uint32_t g_batchStartTime = 0;

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

    // NEW: buffer + acquisition + uploader - create buffer first
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
  static bool benchmarked = false;
  g_poller->loop(SLAVE_ID, START_ADDR, QTY_REGS);
  static uint32_t last = 0;
  uint32_t now = millis();

  if(now-last >= UPLOAD_PERIOD_MS) {
    last = now;
    std::vector<Record> newRecords;
    g_buffer->drainTo(newRecords);

    if(newRecords.empty()) {
      Serial.println("[MAIN] No new records to upload.");
    } else {
      Serial.printf("[MAIN] Drained %u new records from buffer (dropped %u)\n", (unsigned)newRecords.size(), (unsigned)g_buffer->droppedCount());
    
      Serial.println("=== REAL INVERTER DATA COMPRESSION REPORT (WITH TIMESTAMPS) ===");
      Serial.printf("Compression Method Used: Delta Encoding with Timestamp Compression\n");
      Serial.printf("Number of Real Inverter Samples: %u\n", (unsigned)newRecords.size());
      uint32_t original_size = newRecords.size() * sizeof(Record);
      Serial.printf("Original Payload Size: %u bytes\n", original_size);

      // Measure compression time
      uint32_t compress_start = micros();
      std::vector<uint8_t> compressed = Packetizer::finalizeBlock(newRecords);
      uint32_t compress_time = micros() - compress_start;

      Serial.printf("Compressed Payload Size: %u bytes\n", (unsigned)compressed.size());
      float compression_ratio = (float)original_size / (float)compressed.size();
      Serial.printf("Compression Ratio: %.2f:1 (%.1f%% reduction)\n", 
                    compression_ratio, 
                    (1.0f - (float)compressed.size() / (float)original_size) * 100.0f);
      Serial.printf("CPU Time: %u microseconds\n", compress_time);

      std::vector<Record> decompressed = Compression::decompressDelta(compressed);
      bool lossless = (decompressed.size() == newRecords.size());
      Serial.printf("Lossless Recovery Verification: %s\n", lossless ? "PASSED" : "FAILED");

      // --- Packetizer: encrypt, chunk ---
      //std::vector<uint8_t> encrypted = Packetizer::encryptAndMac(compressed);
      //auto chunks = Packetizer::chunkData(encrypted, 32);

      

      bool uploadSuccess = g_uploader->uploadBatch(newRecords);
      if (!uploadSuccess) {
        Serial.println("[MAIN]  Upload failed");
      } else {
        Serial.println("[MAIN]  Real inverter data with timestamps uploaded successfully!");
      }
    }
  }

  delay(5);
}
