/**
 * @file ErrorCodes.h
 * @brief Modbus exception code definitions and error message utilities.
 * 
 * This file provides standardized Modbus exception codes and utility functions
 * for converting error codes into human-readable messages. Based on the official
 * Modbus specification and inverter documentation.
 */

#pragma once
#include <Arduino.h>

/**
 * @namespace ModbusError
 * @brief Namespace containing Modbus exception codes and error handling utilities.
 * 
 * This namespace provides standardized exception codes as defined in the Modbus
 * specification, along with utility functions to convert numeric error codes
 * into descriptive human-readable messages.
 */
namespace ModbusError {

  /**
   * @enum Code
   * @brief Standard Modbus exception codes from the Modbus specification.
   * 
   * These exception codes are returned by Modbus slaves (inverters) to indicate
   * various error conditions during communication. Each code has a specific meaning
   * as defined in the Modbus protocol specification.
   */
  enum Code : uint8_t {
    ILLEGAL_FUNCTION        = 0x01,  ///< Function code not supported by slave
    ILLEGAL_DATA_ADDRESS    = 0x02,  ///< Register address is invalid or out of range
    ILLEGAL_DATA_VALUE      = 0x03,  ///< Data value is out of allowable range
    SLAVE_DEVICE_FAILURE    = 0x04,  ///< Unrecoverable error occurred in slave
    ACKNOWLEDGE             = 0x05,  ///< Slave accepted request but processing is delayed
    SLAVE_DEVICE_BUSY       = 0x06,  ///< Slave is busy processing another command
    MEMORY_PARITY_ERROR     = 0x08,  ///< Memory parity error detected in slave
    GATEWAY_PATH_UNAVAILABLE= 0x0A,  ///< Gateway path to target device unavailable
    GATEWAY_TARGET_FAILED   = 0x0B,  ///< Gateway target device failed to respond
  };

  /**
   * @brief Convert Modbus exception code to human-readable description.
   * 
   * Translates numeric Modbus exception codes into descriptive Flash strings
   * for error reporting and debugging purposes. Uses Arduino's F() macro to
   * store strings in program memory to conserve RAM.
   * 
   * @param code Numeric Modbus exception code (typically 0x01-0x0B)
   * @return Flash string pointer containing descriptive error message
   * 
   * @note Returns "Unknown Modbus Exception" for unrecognized error codes
   */
  inline const __FlashStringHelper* meaning(uint8_t code) {
    switch (code) {
      case ILLEGAL_FUNCTION:         return F("Illegal Function (not supported)");
      case ILLEGAL_DATA_ADDRESS:     return F("Illegal Data Address (invalid)");
      case ILLEGAL_DATA_VALUE:       return F("Illegal Data Value (out of range)");
      case SLAVE_DEVICE_FAILURE:     return F("Slave Device Failure");
      case ACKNOWLEDGE:              return F("Acknowledge (processing delayed)");
      case SLAVE_DEVICE_BUSY:        return F("Slave Device Busy");
      case MEMORY_PARITY_ERROR:      return F("Memory Parity Error");
      case GATEWAY_PATH_UNAVAILABLE: return F("Gateway Path Unavailable");
      case GATEWAY_TARGET_FAILED:    return F("Gateway Target Failed to Respond");
      default:                       return F("Unknown Modbus Exception");
    }
  }

} // namespace ModbusError
