#pragma once
#include <Arduino.h>

/**
 * @file ErrorCodes.h
 * @brief Error code definitions and utilities for Modbus and HTTP communication
 * 
 * This namespace provides error code definitions based on the Modbus specification
 * and simu and emu error codes for HTTP and data parsing operations.
 */

/**
 * @namespace ErrorCodes
 * @brief Simple helper to map Modbus exception codes to human-readable text
 * 
 * Contains error code enumerations and utility functions for translating
 * error codes into descriptive messages.
 */
namespace ErrorCodes {

  /**
   * @enum Code
   * @brief Error codes from Modbus spec, inverter manual, and custom extensions
   * 
   * Standard Modbus exception codes (0x01-0x0B) are defined according to the
   * Modbus protocol specification. Additional codes (0x0C-0x13) are custom
   * extensions for HTTP and frame processing errors.
   */
  enum Code : uint8_t {
    SUCCESS            = 0x00, 
    ILLEGAL_FUNCTION        = 0x01, 
    ILLEGAL_DATA_ADDRESS    = 0x02, 
    ILLEGAL_DATA_VALUE      = 0x03, 
    SLAVE_DEVICE_FAILURE    = 0x04,
    ACKNOWLEDGE             = 0x05, 
    SLAVE_DEVICE_BUSY       = 0x06, 
    MEMORY_PARITY_ERROR     = 0x08, 
    GATEWAY_PATH_UNAVAILABLE= 0x0A, 
    GATEWAY_TARGET_FAILED   = 0x0B, 
    INVALID_FRAME_LENGTH    = 0x0C, 
    HTTP_BEGIN_FAILED        = 0x0D, 
    HTTP_ERROR_CODE          = 0x0E, 
    JSON_PARSE_FRAME_MISSING  = 0x0F,
    BAD_HEX_OR_SHORT_FRAME   = 0x10, 
    CRC_ERROR              = 0x11, 
    HTTP_NEG_ERROR_CODE    = 0x12, 
    BYTE_COUNT_MISMATCH    = 0x13, 
    NO_EXCEPTION          = 0xFE, 
    OTHER                 = 0xFF, 
  };

  /**
   * @brief Translate an exception code into a descriptive string
   * 
   * Converts a numeric error code into a human-readable flash string.
   * Only standard Modbus exception codes are translated; custom codes
   * will return "Unknown Modbus Exception".
   * 
   * @param code The error code to translate
   * @return Pointer to a flash string containing the error description
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

} // namespace ErrorCodes
