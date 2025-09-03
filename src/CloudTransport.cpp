#include "CloudTransport.h"
#include "Modbus.h"

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266HTTPClient.h>
#else
  #include <WiFi.h>
  #include <HTTPClient.h>
#endif

#include <ArduinoJson.h>  // NEW

// Map register address -> display scale and unit (from your table)
static void regScaleUnit(uint16_t addr, float& scale, const char*& unit) {
  switch (addr) {
    case 0:  scale = 10.f;  unit = "V";  break;  // Vac1 / L1 Phase voltage
    case 1:  scale = 10.f;  unit = "A";  break;  // Iac1 / L1 Phase current
    case 2:  scale = 100.f; unit = "Hz"; break;  // Fac1 / L1 Phase frequency
    case 3:  scale = 10.f;  unit = "V";  break;  // Vpv1 / PV1 input voltage
    case 4:  scale = 10.f;  unit = "V";  break;  // Vpv2 / PV2 input voltage
    case 5:  scale = 10.f;  unit = "A";  break;  // Ipv1 / PV1 input current
    case 6:  scale = 10.f;  unit = "A";  break;  // Ipv2 / PV2 input current
    case 7:  scale = 10.f;  unit = "C";  break;  // Inverter internal temperature
    case 8:  scale = 1.f;   unit = "%";  break;  // Export power percentage (R/W)
    case 9:  scale = 1.f;   unit = "W";  break;  // Inverter current output power
    default: scale = 1.f;   unit = "";   break;
  }
}

CloudTransport::CloudTransport(const String& url,
                               const String& auth,
                               uint32_t timeoutMs)
: _url(url), _auth(auth), _timeout(timeoutMs) {}

TransportResult CloudTransport::exchange(const std::vector<uint8_t>& request) {
  TransportResult res;
  HTTPClient http;

#if defined(ESP8266)
  WiFiClient client;
  if (!http.begin(client, _url)) { res.error = F("begin failed"); return res; }
#else
  if (!http.begin(_url)) { res.error = F("begin failed"); return res; }
#endif

  http.setTimeout(_timeout);
  http.addHeader("accept", "*/*");
  if (_auth.length() > 0) http.addHeader("Authorization", _auth);
  http.addHeader("Content-Type", "application/json");

  // Build JSON payload exactly like curl
  String txHex = String(Modbus::toHex(request).c_str());
  txHex.toUpperCase();
  String payload = "{\"frame\":\"" + txHex + "\"}";
  Serial.println("[CloudTransport] Sending payload: " + payload);

  int code = http.POST(payload);
  res.status = code;

  if (code > 0) {
    res.body = http.getString();
    res.ok = (code >= 200 && code < 300);
    if (!res.ok) res.error = "HTTP " + String(code);

    Serial.println("[CloudTransport] HTTP code: " + String(code));
    Serial.println("[CloudTransport] Raw response: " + res.body);

    // Parse JSON and extract "frame"
    StaticJsonDocument<256> doc;
    DeserializationError jerr = deserializeJson(doc, res.body);
    if (!jerr && doc.containsKey("frame")) {
      String rxHex = doc["frame"].as<String>();
      Serial.println("[CloudTransport] Decoded frame HEX: " + rxHex);

      // Hex -> bytes
      std::vector<uint8_t> rx;
      if (Modbus::fromHex(rxHex.c_str(), rx) && rx.size() >= 5) {
        // Print bytes
        Serial.print("[CloudTransport] Frame bytes: ");
        for (auto b : rx) Serial.printf("%02X ", b);
        Serial.println();

        // CRC verify
        uint16_t crcCalc = Modbus::crc16(rx.data(), rx.size() - 2);
        uint16_t crcRecv = rx[rx.size() - 2] | (uint16_t(rx[rx.size() - 1]) << 8);
        if (crcCalc == crcRecv) {
          Serial.println("[CloudTransport] CRC OK");
        } else {
          Serial.printf("[CloudTransport] CRC FAIL (calc=%04X recv=%04X)\n", crcCalc, crcRecv);
        }

        // Recover start address & qty from our original request:
        // request = [slave, 0x03, addr_hi, addr_lo, qty_hi, qty_lo, crc_lo, crc_hi]
        uint16_t startAddr = (uint16_t(request[2]) << 8) | request[3];
        uint16_t qty       = (uint16_t(request[4]) << 8) | request[5];

        uint8_t slave = rx[0];
        uint8_t func  = rx[1];

        if (func == 0x03) {
          if (rx.size() < 5) {
            Serial.println("[CloudTransport] Short 0x03 frame");
          } else {
            uint8_t byteCount = rx[2];
            if (rx.size() < 3 + byteCount + 2) {
              Serial.println("[CloudTransport] ByteCount mismatch");
            } else {
              Serial.printf("[CloudTransport] Slave=0x%02X Func=0x%02X Start=%u Qty=%u\n",
                            slave, func, startAddr, qty);
              // Data words are big-endian, starting at rx[3]
              for (uint16_t i = 0; i < qty && (3 + 2*i + 1) < rx.size(); ++i) {
                uint16_t raw = (uint16_t(rx[3 + 2*i]) << 8) | rx[3 + 2*i + 1];
                uint16_t addr = startAddr + i;
                float scale; const char* unit;
                regScaleUnit(addr, scale, unit);
                float value = raw / scale;
                Serial.printf("  Reg[%u] Addr=%u Raw=0x%04X -> %.3f %s\n",
                              i, addr, raw, value, unit);
              }
            }
          }
        } else if (func & 0x80) {
          // Exception response
          uint8_t ex = rx.size() > 2 ? rx[2] : 0xFF;
          Serial.printf("[CloudTransport] Exception func=0x%02X code=0x%02X\n", func, ex);
        } else {
          Serial.printf("[CloudTransport] Unsupported func=0x%02X\n", func);
        }
      } else {
        Serial.println("[CloudTransport] Bad hex or too short frame");
      }
    } else {
      Serial.println("[CloudTransport] JSON parse error or missing 'frame'");
    }
  } else {
    res.error = http.errorToString(code);
    Serial.println("[CloudTransport] Error: " + res.error);
  }

  http.end();
  return res;
}
