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

//  HTTPClient http;
//#if defined(ESP8266)
//  WiFiClient client;
//  if (!http.begin(client, _apiUrl)) return false;
//#else
//  if (!http.begin(_apiUrl)) return false;
//#endif
//  http.addHeader("accept", "*/*");
//  http.addHeader("Content-Type", "application/octet-stream");
//  if (_auth.length()) http.addHeader("Authorization", _auth);
//
//  // Send hex string for HTTP (server expects hex text in octet-stream)
//  int code = http.POST(hex);
//  _last_http = code;
//  bool ok_http = (code >= 200 && code < 300);
//
//  Serial.printf("[UPLOAD] HTTP Response Code: %d\n", code);
//  Serial.printf("[UPLOAD]  Hex payload sent: %u chars (represents %u bytes)\n", (unsigned)hex.length(), (unsigned)compressed.size());
//
//  // --- Handle server feedback (ACK/config/commands) ---
//  String response;
//  if (ok_http) {
//    response = http.getString();
//    Serial.println("=== SERVER RESPONSE ===");
//    if (response.length()) {
//      Serial.printf("[UPLOAD] Raw Response: %s\n", response.c_str());
//      StaticJsonDocument<256> doc;
//      DeserializationError jerr = deserializeJson(doc, response);
//      if (!jerr) {
//        Serial.printf("[UPLOAD] Server ACK: %s\n", doc["status"].as<const char*>());
//        Serial.printf("[UPLOAD] Server Received: %d bytes\n", doc["received_bytes"].as<int>());
//        if (doc.containsKey("config")) {
//          Serial.printf("[UPLOAD] New Config - upload_interval: %d ms\n", doc["config"]["upload_interval"].as<int>());
//        }
//        if (doc.containsKey("commands")) {
//          Serial.print("[UPLOAD] Server Commands: ");
//          for (JsonVariant v : doc["commands"].as<JsonArray>()) {
//            Serial.printf("%s ", v.as<const char*>());
//          }
//          Serial.println();
//        }
//      } else {
//        Serial.println("[UPLOAD] Failed to parse JSON response");
//      }
//    } else {
//      Serial.println("[UPLOAD] Empty response from server");
//    }
//    Serial.println("=====================");
//  } else {
//    Serial.printf("[UPLOAD] HTTP Error: %d\n", code);
//  }
//  http.end();
//
//  bool ok = ok_mqtt && ok_http;
//  if (ok) { _uploads_ok++; batch.clear(); }
//  else    { _uploads_err++; }
//  return ok;
}

//bool Uploader::uploadRawFrames(const std::vector<Record>& batch) {
//  bool all_ok = true;
//  for (const auto& r : batch) {
//    if (r.raw.size() == 0) continue; // nothing to send in RAW path
//
//    HTTPClient http;
//#if defined(ESP8266)
//    WiFiClient client;
//    if (!http.begin(client, _apiUrl)) return false;
//#else
//    if (!http.begin(_apiUrl)) return false;
//#endif
//    http.addHeader("accept", "*/*");
//    http.addHeader("Content-Type", "application/json");
//    if (_auth.length()) http.addHeader("Authorization", _auth);
//
//    // {"frame":"<hex>"}
//    String payload = String("{\"frame\":\"") + r.raw + "\"}";
//    int code = http.POST(payload);
//    _last_http = code;
//    bool ok = (code >= 200 && code < 300);
//    http.end();
//
//    if (!ok) {
//      all_ok = false;
//      delay(RETRY_DELAY_MS);
//      // Optional: retry per-item up to UPLOAD_MAX_RETRIES here
//    }
//  }
//  return all_ok;
//}

//bool Uploader::uploadDecodedBatch(const std::vector<Record>& batch) {
//  if (String(API_BULK_URL).length() == 0) return false; // no bulk endpoint configured
//
//  // Build a compact JSON batch
//  StaticJsonDocument<1024> root; // if you expect large batches, increase or switch to DynamicJsonDocument
//  root["device_id"] = "ecowatt-esp32";
//  root["fw"] = "1.0.0";
//
//  JsonArray samples = root.createNestedArray("samples");
//  for (const auto& r : batch) {
//    JsonObject s = samples.createNestedObject();
//    s["ts"] = r.ts_ms;       // millis or epoch-ms
//    s["start"] = r.start;
//    s["qty"] = r.qty;
//
//    if (!r.regs.empty()) {
//      JsonArray vals = s.createNestedArray("values");
//      for (const auto& dv : r.regs) {
//        JsonObject v = vals.createNestedObject();
//        v["addr"] = dv.addr;
//        v["raw"]  = dv.raw;
//        v["val"]  = dv.value;
//        v["unit"] = dv.unit;
//      }
//    } else if (r.rawFrameHex.length()) {
//      s["frame"] = r.rawFrameHex;
//    }
//  }
//
//  String payload;
//  serializeJson(root, payload);
//
//  HTTPClient http;
//#if defined(ESP8266)
//  WiFiClient client;
//  if (!http.begin(client, String(API_BULK_URL))) return false;
//#else
//  if (!http.begin(String(API_BULK_URL))) return false;
//#endif
//  http.addHeader("accept", "application/json");
//  http.addHeader("Content-Type", "application/json");
//  if (_auth.length()) http.addHeader("Authorization", _auth);
//
//  int code = http.POST(payload);
//  _last_http = code;
//  bool ok = (code >= 200 && code < 300);
//  http.end();
//  return ok;
//}
