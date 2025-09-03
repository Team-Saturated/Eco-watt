#pragma once
#include <Arduino.h>
#include <vector>
#include <string>

// ---- Shared decoded register type (single source of truth) ----
struct DecodedReg {
  uint16_t addr;   // Modbus register address
  uint16_t raw;    // raw 16-bit value
  float    value;  // engineering value after scaling
  String   unit;   // "V","A","Hz",...
};

// ---- Transport result used by Cloud/RS485 transports ----
struct TransportResult {
  bool ok = false;
  int  status = 0;               // HTTP code or 0 for RS485, negative for lib error
  String error;                  // error text if !ok
  String body;                   // CloudTransport: response body (JSON)
  std::vector<uint8_t> bytes;    // Rs485Transport: raw bytes read
  std::vector<DecodedReg> regs;  // optional: decoded registers (filled by transport or higher layer)
};

// ---- Transport interface (base) ----
class ITransport {
public:
  virtual ~ITransport() = default;

  // Send a Modbus request frame (bytes) and return response/result.
  virtual TransportResult exchange(const std::vector<uint8_t>& request) = 0;
};
