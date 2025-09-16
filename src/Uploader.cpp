#include "Uploader.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

#ifndef API_BULK_URL
// Optional: define in platformio.ini as -DAPI_BULK_URL="\"http://<host>/api/inverter/bulk\""
#define API_BULK_URL ""
#endif

Uploader::Uploader(const String& apiUrl, const String& authHeader)
: _apiUrl(apiUrl), _auth(authHeader) {}

bool Uploader::uploadBatch(std::vector<Record>& batch) {
  if (batch.empty()) return true;

#if (UPLOAD_MODE == UPLOAD_MODE_DECODED)
  bool ok = uploadDecodedBatch(batch);
  if (!ok) {
    // Fallback to RAW per-item if bulk isn’t supported
    ok = uploadRawFrames(batch);
  }
#else
  bool ok = uploadRawFrames(batch);
#endif

  if (ok) { _uploads_ok++; batch.clear(); }
  else    { _uploads_err++; }
  return ok;
}

bool Uploader::uploadRawFrames(const std::vector<Record>& batch) {
  bool all_ok = true;
  for (const auto& r : batch) {
    if (r.rawFrameHex.length() == 0) continue; // nothing to send in RAW path

    HTTPClient http;
#if defined(ESP8266)
    WiFiClient client;
    if (!http.begin(client, _apiUrl)) return false;
#else
    if (!http.begin(_apiUrl)) return false;
#endif
    http.addHeader("accept", "*/*");
    http.addHeader("Content-Type", "application/json");
    if (_auth.length()) http.addHeader("Authorization", _auth);

    // {"frame":"<hex>"}
    String payload = String("{\"frame\":\"") + r.rawFrameHex + "\"}";
    int code = http.POST(payload);
    _last_http = code;
    bool ok = (code >= 200 && code < 300);
    http.end();

    if (!ok) {
      all_ok = false;
      delay(RETRY_DELAY_MS);
      // Optional: retry per-item up to UPLOAD_MAX_RETRIES here
    }
  }
  return all_ok;
}

bool Uploader::uploadDecodedBatch(const std::vector<Record>& batch) {
  if (String(API_BULK_URL).length() == 0) return false; // no bulk endpoint configured

  // Build a compact JSON batch
  StaticJsonDocument<1024> root; // if you expect large batches, increase or switch to DynamicJsonDocument
  root["device_id"] = "ecowatt-esp32";
  root["fw"] = "1.0.0";

  JsonArray samples = root.createNestedArray("samples");
  for (const auto& r : batch) {
    JsonObject s = samples.createNestedObject();
    s["ts"] = r.ts_ms;       // millis or epoch-ms
    s["start"] = r.start;
    s["qty"] = r.qty;

    if (!r.regs.empty()) {
      JsonArray vals = s.createNestedArray("values");
      for (const auto& dv : r.regs) {
        JsonObject v = vals.createNestedObject();
        v["addr"] = dv.addr;
        v["raw"]  = dv.raw;
        v["val"]  = dv.value;
        v["unit"] = dv.unit;
      }
    } else if (r.rawFrameHex.length()) {
      s["frame"] = r.rawFrameHex;
    }
  }

  String payload;
  serializeJson(root, payload);

  HTTPClient http;
#if defined(ESP8266)
  WiFiClient client;
  if (!http.begin(client, String(API_BULK_URL))) return false;
#else
  if (!http.begin(String(API_BULK_URL))) return false;
#endif
  http.addHeader("accept", "application/json");
  http.addHeader("Content-Type", "application/json");
  if (_auth.length()) http.addHeader("Authorization", _auth);

  int code = http.POST(payload);
  _last_http = code;
  bool ok = (code >= 200 && code < 300);
  http.end();
  return ok;
}
