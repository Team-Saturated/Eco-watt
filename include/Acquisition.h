#pragma once
#include <Arduino.h>
#include <vector>
#include "Buffer.h"        // Record, DecodedReg
#include "InverterClient.h"
#include "Modbus.h"

// Acquisition performs a single Modbus 0x03 read and converts it to a Record.
class Acquisition {
public:
  explicit Acquisition(InverterClient& client) : _client(client) {}

  // Do one read. On success, fills 'out' and returns true.
  // 'slave', 'start', 'qty' define what to read.
  bool acquire(uint8_t slave, uint16_t start, uint16_t qty, Record& out, String& err);

private:
  // Fallback decoder for RS485 path when only raw bytes are present.
  bool decode03ToRegs(const std::vector<uint8_t>& rx,
                      uint16_t startAddr, uint16_t qty,
                      std::vector<DecodedReg>& regs, String& err);

  // Scale & unit mapping for addresses you care about (extend as needed).
  void regScaleUnit(uint16_t addr, float& scale, const char*& unit) const;

  InverterClient& _client;
};
