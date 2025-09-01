#pragma once
#include <Arduino.h>
#include <vector>
#include <string>

namespace Modbus {
  uint16_t crc16(const uint8_t* data, size_t len);
  std::vector<uint8_t> buildRead03(uint8_t slave, uint16_t addr, uint16_t qty);
  std::string toHex(const std::vector<uint8_t>& v);
}
