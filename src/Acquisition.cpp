#include "Acquisition.h"
#include <ArduinoJson.h>   // for parsing {"frame":"..."} when only body is available

void Acquisition::regScaleUnit(uint16_t addr, float& scale, const char*& unit) const {
  switch (addr) {
    case 0:  scale = 10.f;  unit = "V";  break;  // Vac1
    case 1:  scale = 10.f;  unit = "A";  break;  // Iac1
    case 2:  scale = 100.f; unit = "Hz"; break;  // Fac1
    case 3:  scale = 10.f;  unit = "V";  break;  // Vpv1
    case 4:  scale = 10.f;  unit = "V";  break;  // Vpv2
    case 5:  scale = 10.f;  unit = "A";  break;  // Ipv1
    case 6:  scale = 10.f;  unit = "A";  break;  // Ipv2
    case 7:  scale = 10.f;  unit = "C";  break;  // Temp
    case 8:  scale = 1.f;   unit = "%";  break;  // Export %
    case 9:  scale = 1.f;   unit = "W";  break;  // Output power
    default: scale = 1.f;   unit = "";   break;
  }
}

bool Acquisition::decode03ToRegs(const std::vector<uint8_t>& rx,
                                 uint16_t startAddr, uint16_t qty,
                                 std::vector<DecodedReg>& regs, String& err) {
  regs.clear();
  if (rx.size() < 5) { err = F("short frame"); return false; }

  // CRC check (last two bytes are CRC LSB/MSB)
  uint16_t crcCalc = Modbus::crc16(rx.data(), rx.size() - 2);
  uint16_t crcRecv = rx[rx.size()-2] | (uint16_t(rx[rx.size()-1]) << 8);
  if (crcCalc != crcRecv) { err = F("crc mismatch"); /* keep parsing anyway */ }

  uint8_t func = rx[1];
  if (func & 0x80) { err = F("exception response"); return false; }
  if (func != 0x03) { err = F("unexpected func");  return false; }

  uint8_t byteCount = rx[2];
  if (rx.size() < 3 + byteCount + 2) { err = F("bytecount mismatch"); return false; }

  // data words start at rx[3], big-endian
  for (uint16_t i = 0; i < qty && (3 + 2*i + 1) < rx.size(); ++i) {
    uint16_t raw = (uint16_t(rx[3 + 2*i]) << 8) | rx[3 + 2*i + 1];
    uint16_t addr = startAddr + i;
    float scale; const char* unit;
    regScaleUnit(addr, scale, unit);
    float value = raw / scale;
    regs.push_back(DecodedReg{addr, raw, value, String(unit)});
  }
  return true;
}

bool Acquisition::acquire(uint8_t slave, uint16_t start, uint16_t qty, Record& out, String& err) {
  err = "";
  out = Record{};
  out.ts_ms = millis();
  out.start = start;
  out.qty   = qty;

  // Perform read via client/transport
  auto tr = _client.readHolding(slave, start, qty);
  if (!tr.ok) {
    err = tr.error.length() ? tr.error : String(F("transport error"));
    return false;
  }

  // 1) Prefer raw bytes if available: set rawFrameHex from bytes
  if (!tr.bytes.empty()) {
    out.rawFrameHex = String(Modbus::toHex(tr.bytes).c_str());
  }

  // 2) If transport already decoded, pass through
  if (!tr.regs.empty()) {
    out.regs = tr.regs;
    return true;
  }

  // 3) If no bytes but a body exists, try to extract "frame" from JSON and decode
  if (tr.bytes.empty() && !tr.body.isEmpty()) {
    StaticJsonDocument<256> doc;
    DeserializationError jerr = deserializeJson(doc, tr.body);
    if (!jerr && doc.containsKey("frame")) {
      String frameHex = doc["frame"].as<String>();
      out.rawFrameHex = frameHex; // keep the clean hex

      std::vector<uint8_t> rx;
      if (Modbus::fromHex(frameHex.c_str(), rx)) {
        std::vector<DecodedReg> regs;
        String derr;
        if (decode03ToRegs(rx, start, qty, regs, derr)) {
          out.regs = std::move(regs);
          return true;
        } else {
          err = derr.length() ? derr : String(F("decode failed"));
          return false;
        }
      } else {
        err = F("bad frame hex");
        return false;
      }
    }
    // if body didn’t contain "frame", fall through to error
  }

  // 4) Nothing usable
  err = F("no data");
  return false;
}
