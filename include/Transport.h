/**
 * @file Transport.h
 * @brief Transport layer abstraction for Modbus communication protocols.
 * 
 * This file defines the transport layer interface and data structures used
 * for communicating with solar inverters through various protocols (RS485,
 * HTTP/Cloud APIs, etc.). It provides a unified interface for different
 * transport implementations.
 */

#pragma once
#include <Arduino.h>
#include <vector>

/**
 * @enum ErrType
 * @brief Error classification enumeration for actionable error handling.
 * 
 * Categorizes different types of errors that can occur during transport
 * operations, enabling appropriate error handling and recovery strategies.
 */
enum class ErrType : uint8_t {
  NONE = 0,     ///< No error occurred, operation successful
  HTTP,         ///< HTTP protocol error (network, server response, etc.)
  JSON,         ///< JSON parsing or format error
  CRC,          ///< CRC checksum mismatch in received data
  TIMEOUT,      ///< Operation timed out waiting for response
  NO_DATA,      ///< No data received or empty response
  MODBUS_EXC,   ///< Modbus exception response (func|0x80 with exception code)
  OTHER         ///< Other unclassified errors
};

/**
 * @struct DecodedReg
 * @brief Represents a single decoded Modbus register with engineering values.
 * 
 * This structure serves as the single source of truth for decoded register
 * data, containing both raw Modbus values and converted engineering units
 * for human interpretation.
 */
struct DecodedReg {
  uint16_t addr;   ///< Modbus register address (e.g., 0x0001, 0x0002)
  uint16_t raw;    ///< Raw 16-bit value as received from Modbus
  float    value;  ///< Engineering value after applying scale factor
  String   unit;   ///< Engineering unit string (e.g., "V", "A", "Hz", "W")
};

/**
 * @struct TransportResult
 * @brief Comprehensive result structure for transport operations.
 * 
 * Contains all information from a transport operation including success status,
 * error classification, raw data, decoded registers, and protocol-specific
 * details. Used by both Cloud and RS485 transport implementations.
 */
struct TransportResult {
  bool ok = false;                    ///< Overall operation success flag

  // Status and classification
  int     status = 0;                 ///< HTTP status code (cloud) or 0 for RS485; negative for library errors
  ErrType type  = ErrType::NONE;      ///< Error type classification for handling

  // Optional error details
  String  error;                      ///< Human-readable error description when !ok
  String  body;                       ///< Raw response body (CloudTransport: JSON string)

  // Response data
  std::vector<uint8_t>   bytes;       ///< Raw response bytes (Modbus RTU/TCP payload including CRC)
  std::vector<DecodedReg> regs;       ///< Decoded registers (filled by transport or higher layer)

  // Modbus exception information (valid when type==MODBUS_EXC)
  int exc_code = -1;                  ///< Modbus exception code (0x01-0x0B), -1 if not applicable
};

/**
 * @class ITransport
 * @brief Abstract base class defining the transport layer interface.
 * 
 * This interface provides a unified abstraction for different transport
 * protocols used to communicate with solar inverters. Implementations
 * include RS485 Modbus RTU and HTTP-based cloud APIs.
 */
class ITransport {
public:
  /**
   * @brief Virtual destructor for proper cleanup of derived classes.
   */
  virtual ~ITransport() = default;

  /**
   * @brief Exchange Modbus request/response through the transport layer.
   * 
   * Sends a complete Modbus request frame and receives the response through
   * the specific transport implementation (RS485, HTTP, etc.). The request
   * should be a properly formatted Modbus frame with CRC for RTU transports.
   * 
   * @param request Complete Modbus request frame as byte vector
   * @return TransportResult containing response data, status, and error information
   */
  virtual TransportResult exchange(const std::vector<uint8_t>& request) = 0;
};
