#include "CloudTransport.h"
#include "Modbus.h"
#include "ErrorCodes.h"   // friendly Modbus exception text
#include "Transport.h"    // for ErrType

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266HTTPClient.h>
#else
  #include <WiFi.h>
  #include <HTTPClient.h>
#endif

#include <ArduinoJson.h>

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
  if (!http.begin(client, _url)) {
    res.ok = false; res.type = ErrType::OTHER;
    res.error = F("begin failed"); 
    return res;
  }
#else
  if (!http.begin(_url)) {
    res.ok = false; res.type = ErrType::OTHER;
    res.error = F("begin failed");
    return res;
  }
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
    if (!res.ok) {
      res.type = ErrType::HTTP;
      res.error = "HTTP " + String(code);
      Serial.println("[CloudTransport] HTTP code: " + String(code));
      Serial.println("[CloudTransport] Raw response: " + res.body);
      http.end();
      return res; // stop on non-2xx
    }

    Serial.println("[CloudTransport] HTTP code: " + String(code));
    Serial.println("[CloudTransport] Raw response: " + res.body);

    // Parse JSON and extract "frame"
    StaticJsonDocument<256> doc;
    DeserializationError jerr = deserializeJson(doc, res.body);
    if (jerr || !doc.containsKey("frame")) {
      res.ok = false; res.type = ErrType::JSON;
      res.error = F("json parse/missing 'frame'");
      Serial.println("[CloudTransport] JSON parse error or missing 'frame'");
      http.end();
      return res;
    }

    String rxHex = doc["frame"].as<String>();
    Serial.println("[CloudTransport] Decoded frame HEX: " + rxHex);

    // Hex -> bytes
    std::vector<uint8_t> rx;
    if (!Modbus::fromHex(rxHex.c_str(), rx) || rx.size() < 5) {
      res.ok = false; res.type = ErrType::OTHER;
      res.error = F("bad hex or short frame");
      Serial.println("[CloudTransport] Bad hex or too short frame");
      http.end();
      return res;
    }

    // Keep raw bytes
    res.bytes = rx;

    // Print bytes
    Serial.print("[CloudTransport] Frame bytes: ");
    for (auto b : rx) Serial.printf("%02X ", b);
    Serial.println();

    // CRC verify
    uint16_t crcCalc = Modbus::crc16(rx.data(), rx.size() - 2);
    uint16_t crcRecv = rx[rx.size() - 2] | (uint16_t(rx[rx.size() - 1]) << 8);
    if (crcCalc != crcRecv) {
      res.ok = false; res.type = ErrType::CRC;
      res.error = "crc mismatch calc=" + String(crcCalc, HEX) + " recv=" + String(crcRecv, HEX);
      Serial.printf("[CloudTransport] CRC FAIL (calc=%04X recv=%04X)\n", crcCalc, crcRecv);
      http.end();
      return res;
    }
    Serial.println("[CloudTransport] CRC OK");

    // Recover start address & qty from our original request:
    // request = [slave, 0x03, addr_hi, addr_lo, qty_hi, qty_lo, crc_lo, crc_hi]
    uint16_t startAddr = (uint16_t(request[2]) << 8) | request[3];
    uint16_t qty       = (uint16_t(request[4]) << 8) | request[5];

    uint8_t slave = rx[0];
    uint8_t func  = rx[1];

    if (func == 0x03) {
      if (rx.size() < 5) {
        res.ok = false; res.type = ErrType::OTHER;
        res.error = F("short 0x03 frame");
        Serial.println("[CloudTransport] Short 0x03 frame");
        http.end(); return res;
      }
      uint8_t byteCount = rx[2];
      if (rx.size() < 3 + byteCount + 2) {
        res.ok = false; res.type = ErrType::OTHER;
        res.error = F("bytecount mismatch");
        Serial.println("[CloudTransport] ByteCount mismatch");
        http.end(); return res;
      }

      Serial.printf("[CloudTransport] Slave=0x%02X Func=0x%02X Start=%u Qty=%u\n",
                    slave, func, startAddr, qty);

      // Data words are big-endian, starting at rx[3]
      for (uint16_t i = 0; i < qty && (3 + 2*i + 1) < rx.size(); ++i) {
        uint16_t raw = (uint16_t(rx[3 + 2*i]) << 8) | rx[3 + 2*i + 1];
        uint16_t addr = startAddr + i;
        float scale; const char* unit;
        regScaleUnit(addr, scale, unit);
        float value = raw / scale;

        // Print
        Serial.printf("  Reg[%u] Addr=%u Raw=0x%04X -> %.3f %s\n",
                      i, addr, raw, value, unit);

        // Store decoded register in result
        res.regs.push_back(DecodedReg{addr, raw, value, String(unit)});
      }

      // Success path
      res.ok = true; 
      res.type = ErrType::NONE;
    }
    else if (func & 0x80) {
      // Exception response
      uint8_t ex = rx.size() > 2 ? rx[2] : 0xFF;
      const char* meaning = reinterpret_cast<const char*>(ModbusError::meaning(ex));
      Serial.printf("[CloudTransport] Exception func=0x%02X code=0x%02X (%s)\n",
                    func, ex, meaning);
      res.ok = false;
      res.type = ErrType::MODBUS_EXC;
      res.exc_code = ex;
      res.error = String("modbus exception 0x") + String(ex, HEX) + " (" + meaning + ")";
    }
    else {
      res.ok = false; res.type = ErrType::OTHER;
      res.error = String("unsupported func=0x") + String(func, HEX);
      Serial.printf("[CloudTransport] Unsupported func=0x%02X\n", func);
    }
  } else {
    // POST failed at transport level (negative codes mean client errors)
    res.ok = false;
    res.error = http.errorToString(code);

    // Classify common HTTPClient negative errors
    // Arduino-HTTPClient defines:
    //  -11 READ_TIMEOUT, -5 CONNECTION_LOST, -4 NOT_CONNECTED, etc.
    #ifdef HTTPC_ERROR_READ_TIMEOUT
    if (code == HTTPC_ERROR_READ_TIMEOUT) res.type = ErrType::TIMEOUT;
    else
    #endif
    res.type = ErrType::OTHER;

    Serial.println("[CloudTransport] Error: " + res.error);
  }

  http.end();
  return res;
}
