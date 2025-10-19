#pragma once
#include "Transport.h"

/**
 * @file CloudTransport.h
 * @brief Cloud-based transport layer implementation for simulation purposes.
 */

/**
 * @class CloudTransport
 * @brief Implements cloud-based transport protocol for data exchange
 * 
 * This class provides functionality to communicate with InverterSim
 * using separate read and write endpoints with authentication support.
 */
class CloudTransport : public ITransport {
public:
  /**
   * @brief Constructs a CloudTransport instance
   * 
   * @param read_url URL endpoint for read registers
   * @param write_url URL endpoint for write registers
   * @param auth Authentication key
   * @param timeoutMs Timeout duration in milliseconds for cloud requests
   */
  CloudTransport(const String& read_url, const String& write_url, const String& auth, uint32_t timeoutMs);
  
  /**
   * @brief Exchanges data with the cloud service
   * 
   * @param request Vector of bytes containing the request data frame(Modbus)
   * @param isWrite Flag indicating if this is a write operation (true) or read operation (false)
   * @return TransportResult Result of the exchange operation
   */
  TransportResult exchange(const std::vector<uint8_t>& request, const bool isWrite) override;

private:
  String _read_url;      ///< URL for read operations
  String _write_url;     ///< URL for write operations
  String _auth;          ///< Authentication credentials
  uint32_t _timeout;     ///< Request timeout in milliseconds
};

/// Received Write Emulation Command Data
extern bool writeemulationreceived;
extern uint8_t FUNCTION_CODE;
extern String ERROR_TYPE;
extern uint8_t EXCEPTION_CODE;
extern uint16_t DELAY_MS;