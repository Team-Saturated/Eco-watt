#pragma once
#include <Arduino.h>
#include <vector>

// ---- Error classification for actionable handling ----
enum class ErrType : uint8_t {
  NONE = 0,
  HTTP,
  JSON,
  CRC,
  TIMEOUT,
  NO_DATA,
  MODBUS_EXC,   // func|0x80 with exception code in exc_code
  OTHER
};

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

  // Status + classification
  int     status = 0;         // HTTP code (cloud) or 0 for RS485; negative for lib error
  ErrType type  = ErrType::NONE;

  // Optional details
  String  error;              // human-readable error if !ok
  String  body;               // CloudTransport: raw response (e.g., JSON)

  // Data
  std::vector<uint8_t>   bytes;   // raw response bytes (Modbus RTU/TCP payload including CRC)
  std::vector<DecodedReg> regs;   // optional: decoded registers (filled by transport or higher layer)

  // Modbus exception info (valid when type==MODBUS_EXC)
  int exc_code = -1;              // 0x01..0x0B, -1 if N/A
};

// ---- Transport interface (base) ----
class ITransport {
public:
  virtual ~ITransport() = default;

  // Send a Modbus request frame (bytes) and return response/result.
  virtual TransportResult exchange(const std::vector<uint8_t>& request) = 0;
};
