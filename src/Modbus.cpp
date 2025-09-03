#include "Modbus.h"

namespace {
uint16_t crc16_modbus(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i=0;i<len;++i) {
    crc ^= data[i];
    for (int j=0;j<8;++j) {
      bool lsb = crc & 1;
      crc >>= 1;
      if (lsb) crc ^= 0xA001;
    }
  }
  return crc;
}
}

namespace Modbus {
uint16_t crc16(const uint8_t* data, size_t len){ return crc16_modbus(data, len); }

std::vector<uint8_t> buildRead03(uint8_t slave, uint16_t addr, uint16_t qty) {
  // Input validation
  if (qty > 125) {  // Maximum number of registers per Modbus spec
    qty = 125;
  }
  
  std::vector<uint8_t> f = {
    slave, 0x03,  // Function code 0x03 = Read Holding Registers
    (uint8_t)(addr>>8), (uint8_t)(addr&0xFF),  // Starting address
    (uint8_t)(qty>>8),  (uint8_t)(qty&0xFF)    // Quantity of registers
  };
  
  // Calculate and append CRC
  auto crc = crc16((uint8_t*)f.data(), f.size());
  f.push_back((uint8_t)(crc & 0xFF));   // LSB first
  f.push_back((uint8_t)(crc >> 8));     // MSB second
  return f;
}

std::string toHex(const std::vector<uint8_t>& v) {
  std::string s; s.reserve(v.size()*2);
  const char* hex="0123456789ABCDEF";
  for (auto b: v){ s += hex[b>>4]; s += hex[b&0xF]; }
  return s;
}
}
