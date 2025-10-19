#include "Modbus.h"
#include <cstring>  // for strlen
#include "ErrorCodes.h"
namespace {
uint16_t crc16_modbus(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int j = 0; j < 8; ++j) {
      bool lsb = crc & 1;
      crc >>= 1;
      if (lsb) crc ^= 0xA001;
    }
  }
  return crc;
}

inline int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  return -1;
}
}

namespace Modbus {

uint16_t crc16(const uint8_t* data, size_t len) {
  return crc16_modbus(data, len);
}

std::vector<uint8_t> buildRead03(uint8_t slave, uint16_t addr, uint16_t qty) {
  // Input validation
  if (qty > 125) {  // Maximum number of registers per Modbus spec
    qty = 125;
  }

  std::vector<uint8_t> f = {
    slave, 0x03,  // Function code 0x03 = Read Holding Registers
    (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF),  // Starting address
    (uint8_t)(qty >> 8),  (uint8_t)(qty & 0xFF)    // Quantity of registers
  };

  // Calculate and append CRC
  auto crc = crc16((uint8_t*)f.data(), f.size());
  f.push_back((uint8_t)(crc & 0xFF));   // LSB first
  f.push_back((uint8_t)(crc >> 8));     // MSB second
  return f;
}

std::vector<uint8_t> buildWrite05(uint8_t slave, uint16_t addr, uint16_t value) {
  std::vector<uint8_t> f = {
    slave, 0x06,  // Function code 0x05 = Write Single Coil
    (uint8_t)(addr >> 8), (uint8_t)(addr & 0xFF),  // Coil address
    (uint8_t)(value >> 8),  (uint8_t)(value & 0xFF) // Value to write (0x0000 or 0xFF00)
  };

  // Calculate and append CRC
  auto crc = crc16((uint8_t*)f.data(), f.size());
  f.push_back((uint8_t)(crc & 0xFF));   // LSB first
  f.push_back((uint8_t)(crc >> 8));     // MSB second
  return f;
}

std::string toHex(const std::vector<uint8_t>& v) {
  std::string s;
  s.reserve(v.size() * 2);
  const char* hex = "0123456789ABCDEF";
  for (auto b : v) {
    s += hex[b >> 4];
    s += hex[b & 0xF];
  }
  return s;
}

bool fromHex(const char* hex, std::vector<uint8_t>& out) {
  out.clear();
  if (!hex) return false;
  size_t n = strlen(hex);
  if (n % 2 != 0) return false;
  out.reserve(n / 2);
  for (size_t i = 0; i < n; i += 2) {
    int hi = hexVal(hex[i]);
    int lo = hexVal(hex[i + 1]);
    if (hi < 0 || lo < 0) return false;
    out.push_back(uint8_t((hi << 4) | lo));
  }
  return true;
}

ErrorCodes::Code getModbusErrorMessage(const std::vector<uint8_t>& rx) 
{
      // Minimum valid Modbus exception response length (address + func + code + CRC)
      if (rx.size() < 5) 
      {
          return ErrorCodes::INVALID_FRAME_LENGTH;
      }

      // Function code: bit7 set means exception (error)
      uint8_t funcCode = rx[1];
      bool isException = funcCode & 0x80;

      if (!isException) 
      {
          return ErrorCodes::NO_EXCEPTION;
      }

      // Extract exception code
      uint8_t exCode = rx[2];
      
      switch (exCode) 
      {
          case 0x01: return ErrorCodes::ILLEGAL_FUNCTION;
          case 0x02: return ErrorCodes::ILLEGAL_DATA_ADDRESS;
          case 0x03: return ErrorCodes::ILLEGAL_DATA_VALUE;
          case 0x04: return ErrorCodes::SLAVE_DEVICE_FAILURE;
          case 0x05: return ErrorCodes::ACKNOWLEDGE;
          case 0x06: return ErrorCodes::SLAVE_DEVICE_BUSY;
          case 0x08: return ErrorCodes::MEMORY_PARITY_ERROR;
          case 0x0A: return ErrorCodes::GATEWAY_PATH_UNAVAILABLE;
          case 0x0B: return ErrorCodes::GATEWAY_TARGET_FAILED;
          default:   return ErrorCodes::NO_EXCEPTION;
      }
}


} // namespace Modbus
