#pragma once
#include <Arduino.h>
#include <vector>
#include <string>
#include "ErrorCodes.h"

/**
 * @brief Modbus RTU communication utilities
 * 
 * This namespace provides functions for building Modbus RTU frames,
 * calculating CRC, and parsing responses.
 */
namespace Modbus {
  /**
   * @brief Calculate Modbus CRC16
   * 
   * @param data Pointer to data buffer
   * @param len Length of data buffer
   * @return uint16_t Calculated CRC16 value (little-endian)
   */
  uint16_t crc16(const uint8_t* data, size_t len);
  
  /**
   * @brief Build Modbus function 0x03 (Read Holding Registers) request
   * 
   * @param slave Slave address (0x11)
   * @param addr Starting register address (0-65535)
   * @param qty Quantity of registers to read (1-125)
   * @return std::vector<uint8_t> Complete Modbus request frame with CRC
   */
  std::vector<uint8_t> buildRead03(uint8_t slave, uint16_t addr, uint16_t qty);
  
  /**
   * @brief Build Modbus function 0x05 (Write Single Coil) request
   * 
   * @param slave Slave address (1-247)
   * @param addr Coil address (0-65535)
   * @param value Coil value (0x0000 = OFF, 0xFF00 = ON)
   * @return std::vector<uint8_t> Complete Modbus request frame with CRC
   */
  std::vector<uint8_t> buildWrite05(uint8_t slave, uint16_t addr, uint16_t value);

  /**
   * @brief Convert byte vector to hexadecimal string
   * 
   * @param v Vector of bytes to convert
   * @return std::string Hexadecimal string representation (e.g., "110300000002C69B")
   */
  std::string toHex(const std::vector<uint8_t>& v);

  /**
   * @brief Convert hexadecimal string to byte vector
   * 
   * @param hex C-string of hex characters (e.g., "110300000002C69B")
   * @param out Output vector to fill with bytes
   * @return true Conversion successful
   * @return false Invalid input (odd length or non-hex characters)
   */
  bool fromHex(const char* hex, std::vector<uint8_t>& out);

  /**
   * @brief Extract error code from Modbus response
   * 
   * Checks if the response contains a Modbus exception and returns
   * the corresponding error code.
   * 
   * @param rx Received Modbus response frame
   * @return ErrorCodes::Code Error code (OK if no error, specific code otherwise)
   */
  ErrorCodes::Code getModbusErrorMessage(const std::vector<uint8_t>& rx);

}
