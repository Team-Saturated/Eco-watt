#include "CloudTransport.h"
#include "Modbus.h"

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266HTTPClient.h>
#else
  #include <WiFi.h>
  #include <HTTPClient.h>
#endif

CloudTransport::CloudTransport(const String& url,
                               const String& auth,
                               uint32_t timeoutMs)
: _url(url), _auth(auth), _timeout(timeoutMs) {}

TransportResult CloudTransport::exchange(const std::vector<uint8_t>& request) {
  TransportResult res;
  HTTPClient http;

#if defined(ESP8266)
  WiFiClient client;
  if (!http.begin(client, _url)) {
    res.error = F("begin failed");
    return res;
  }
#else
  if (!http.begin(_url)) {
    res.error = F("begin failed");
    return res;
  }
#endif

  http.setTimeout(_timeout);
  http.addHeader("accept", "*/*");
  if (_auth.length() > 0) {
    http.addHeader("Authorization", _auth);
  }
  http.addHeader("Content-Type", "application/json");

  // Build JSON payload
  String payload = "{\"frame\":\"" + String(Modbus::toHex(request).c_str()) + "\"}";

  int code = http.POST(payload);
  res.status = code;

  if (code > 0) {
    res.body = http.getString();
    res.ok = (code >= 200 && code < 300);
    if (!res.ok) {
      res.error = "HTTP " + String(code);
    }
  } else {
    res.error = http.errorToString(code);
  }

  http.end();
  return res;
}
