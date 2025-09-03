#pragma once
#include <Arduino.h>
#include <vector>
#include <string>

namespace Modbus {
  // Calculate Modbus CRC16
  uint16_t crc16(const uint8_t* data, size_t len);
  
  // Build Modbus function 0x03 (Read Holding Registers) request
  // @param slave: Slave address (1-247)
  // @param addr: Starting address (0-65535)
  // @param qty: Quantity of registers to read (1-125)
  std::vector<uint8_t> buildRead03(uint8_t slave, uint16_t addr, uint16_t qty);
  
  // Convert byte vector to hexadecimal string
  std::string toHex(const std::vector<uint8_t>& v);

  // Convert hexadecimal string to byte vector
  // @param hex: C-string of hex chars (e.g. "110300000002C69B")
  // @param out: vector<uint8_t> to fill
  // @return true if successful, false on invalid input
  bool fromHex(const char* hex, std::vector<uint8_t>& out);
}
