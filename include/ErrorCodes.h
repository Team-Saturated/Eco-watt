#pragma once
#include <Arduino.h>

// Simple helper to map Modbus exception codes to human-readable text
namespace ModbusError {

  // Exception codes from the Modbus spec / inverter manual
  enum Code : uint8_t {
    ILLEGAL_FUNCTION        = 0x01,
    ILLEGAL_DATA_ADDRESS    = 0x02,
    ILLEGAL_DATA_VALUE      = 0x03,
    SLAVE_DEVICE_FAILURE    = 0x04,
    ACKNOWLEDGE             = 0x05,
    SLAVE_DEVICE_BUSY       = 0x06,
    MEMORY_PARITY_ERROR     = 0x08,
    GATEWAY_PATH_UNAVAILABLE= 0x0A,
    GATEWAY_TARGET_FAILED   = 0x0B,
  };

  // Translate an exception code into a descriptive string
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
