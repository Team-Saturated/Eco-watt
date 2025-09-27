/**
 * @file Modbus.h
 * @brief Modbus protocol utilities and frame processing functions.
 * 
 * This file provides low-level Modbus protocol utilities including CRC calculation,
 * frame construction for reading holding registers, and data conversion functions
 * for hexadecimal encoding/decoding.
 */

#pragma once
#include <Arduino.h>
#include <vector>
#include <string>

/**
 * @namespace Modbus
 * @brief Namespace containing Modbus protocol utilities and helper functions.
 * 
 * This namespace provides essential Modbus protocol functions including CRC16
 * calculation, frame construction for reading operations, and data format
 * conversion utilities for debugging and communication.
 */
namespace Modbus {
  
  /**
   * @brief Calculate Modbus CRC16 checksum for data integrity.
   * 
   * Computes the standard Modbus CRC16 checksum used for error detection
   * in Modbus RTU frames. Uses the polynomial 0xA001 (reverse of 0x8005).
   * 
   * @param data Pointer to byte array to calculate CRC for
   * @param len Number of bytes in the data array
   * @return 16-bit CRC checksum value (little-endian format)
   */
  uint16_t crc16(const uint8_t* data, size_t len);
  
  /**
   * @brief Build Modbus Function Code 0x03 (Read Holding Registers) request frame.
   * 
   * Constructs a complete Modbus RTU frame for reading holding registers,
   * including slave address, function code, register address, quantity,
   * and CRC16 checksum.
   * 
   * @param slave Modbus slave address (valid range: 1-247)
   * @param addr Starting register address (valid range: 0-65535)
   * @param qty Number of consecutive registers to read (valid range: 1-125)
   * @return Complete Modbus RTU frame as byte vector, ready for transmission
   */
  std::vector<uint8_t> buildRead03(uint8_t slave, uint16_t addr, uint16_t qty);
  
  /**
   * @brief Convert byte vector to hexadecimal string representation.
   * 
   * Transforms a vector of bytes into a hexadecimal string format for
   * debugging, logging, or human-readable display purposes.
   * 
   * @param v Byte vector to convert
   * @return Hexadecimal string (e.g., "110300000002C69B" for a typical frame)
   */
  std::string toHex(const std::vector<uint8_t>& v);

  /**
   * @brief Convert hexadecimal string to byte vector.
   * 
   * Parses a hexadecimal string and converts it back into a byte vector.
   * Useful for processing hex-encoded data received from APIs or stored
   * in configuration files.
   * 
   * @param hex C-string containing hexadecimal characters (e.g., "110300000002C69B")
   * @param out Output vector to fill with decoded bytes
   * @return true if parsing succeeded, false if input contains invalid characters
   *         or has odd length
   */
  bool fromHex(const char* hex, std::vector<uint8_t>& out);
}
