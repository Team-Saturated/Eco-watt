#include "Uploader.h"
#include "Mqtt.h"
#if defined(ESP8266)
  #include <ESP8266HTTPClient.h>
#else
  #include <HTTPClient.h>
#endif
#include <ArduinoJson.h>
#include "Compression.h" // Added for compression

#ifndef API_BULK_URL
// Optional: define in platformio.ini as -DAPI_BULK_URL="\"http://<host>/api/inverter/bulk\""
#define API_BULK_URL ""
#endif

Uploader::Uploader(const String& apiUrl, const String& authHeader)
: _apiUrl(apiUrl), _auth(authHeader) {}

bool Uploader::uploadBatch(std::vector<Record>& batch) {

  if (batch.empty()) return true;

  // Show timestamp information for the batch
  uint64_t first_ts = batch.front().ts_ms;
  uint64_t last_ts = batch.back().ts_ms;
  Serial.printf("[UPLOAD]  Uploading batch with timestamps: %llu to %llu ms\n", first_ts, last_ts);
  Serial.printf("[UPLOAD]  Batch contains %d records spanning %llu ms\n", 
               (int)batch.size(), (last_ts - first_ts));

  // --- Compress batch before upload ---
  std::vector<uint8_t> compressed = Compression::compressDelta(batch);
  
  // Build hex once (for MQTT JSON only)
  String hex;
  hex.reserve(compressed.size() * 2);
  for (uint8_t b : compressed) {
    char buf[3];
    snprintf(buf, sizeof(buf), "%02X", b);
    hex += buf;
  }

  // Build JSON payload for MQTT so subscribers (e.g., MQTTX) can parse it
  String mqttJson;
  mqttJson.reserve(hex.length() + 256);
  mqttJson += "{";
  mqttJson += "\"type\":\"delta_v1_hex\",";
  mqttJson += "\"timestamp\":"; 
  {
    char tsbuf[32];
    snprintf(tsbuf, sizeof(tsbuf), "%llu", (unsigned long long)last_ts);
    mqttJson += tsbuf;
  }
  mqttJson += ",";
  mqttJson += "\"records\":"; mqttJson += String((int)batch.size()); mqttJson += ",";
  mqttJson += "\"compressed_bytes\":"; mqttJson += String((int)compressed.size()); mqttJson += ",";
  mqttJson += "\"payload_hex\":\""; mqttJson += hex; mqttJson += "\"";
  mqttJson += "}";

  // Ensure PubSubClient buffer is large enough, then publish JSON
  if (mqttJson.length() > 0) {
    client.setBufferSize((uint16_t)(mqttJson.length() + 64));
  }
  bool ok_mqtt = client.publish(t_data.c_str(), mqttJson.c_str(), false); // retain=false
  
  // Serial.printf(compressed.data()); // Removed: unsafe to print raw binary as string
  // Optionally, print first few bytes as hex for debugging:
  Serial.print("[UPLOAD] Compressed data (first 8 bytes): ");
  for (size_t i = 0; i < compressed.size() && i < 8; ++i) {
    Serial.printf("%02X ", compressed[i]);
  }
  Serial.println();

  // For benchmarking, you can compare compressed.size() vs. batch.size()*sizeof(Record)
  Serial.printf("[UPLOAD] Compressed batch size: %u bytes (includes timestamps)\n", (unsigned)compressed.size());
  Serial.printf("[UPLOAD] MQTT JSON length: %u bytes\n", (unsigned)mqttJson.length());

}
