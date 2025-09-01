#include "CloudTransport.h"
#include "Modbus.h"

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266HTTPClient.h>
#else
  #include <WiFi.h>
  #include <HTTPClient.h>
#endif

CloudTransport::CloudTransport(const std::string& url,
                               const std::string& auth,
                               uint32_t timeoutMs)
: _url(url), _auth(auth), _timeout(timeoutMs) {}

CloudTransport::CloudTransport(const String& url,
                               const String& auth,
                               uint32_t timeoutMs)
: _url(url.c_str()), _auth(auth.c_str()), _timeout(timeoutMs) {}

TransportResult CloudTransport::exchange(const std::vector<uint8_t>& request) {
  TransportResult res;
  HTTPClient http;

  // HTTPClient expects Arduino String
  String urlS(_url.c_str());
  if (!http.begin(urlS)) { res.error = "begin failed"; return res; }

  http.setTimeout(_timeout);
  http.addHeader("Content-Type", "application/json");
  if (!_auth.empty()) http.addHeader("Authorization", String(_auth.c_str()));

  // Build JSON payload. If your Modbus::toHex returns std::string, use .c_str().
  // (If yours returns Arduino String, you can drop the .c_str() wrapper below.)
  String payload = String("{\"frame\":\"") + String(Modbus::toHex(request).c_str()) + "\"}";

  int code = http.POST(payload);
  res.status = code;

  if (code > 0) {
    res.body = std::string(http.getString().c_str());            // String -> std::string
    res.ok   = (code >= 200 && code < 300);
    if (!res.ok) res.error = std::string("HTTP ") + std::to_string(code);
  } else {
    res.error = std::string(http.errorToString(code).c_str());   // String -> std::string
  }

  http.end();
  return res;
}
