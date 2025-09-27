/**
 * @file InverterClient.h
 * @brief High-level interface for communicating with solar inverters.
 * 
 * This file defines the InverterClient class which provides a simplified
 * interface for reading data from solar inverters using various transport
 * protocols (RS485, HTTP, etc.).
 */

#pragma once
#include "Transport.h"
#include <vector>

/**
 * @class InverterClient
 * @brief High-level client for inverter communication operations.
 * 
 * The InverterClient class abstracts the complexity of different transport
 * protocols (RS485 Modbus, HTTP APIs, etc.) and provides a unified interface
 * for reading holding registers from solar inverters.
 */
class InverterClient {
public:
  /**
   * @brief Construct a new InverterClient object.
   * 
   * @param t Reference to the transport layer implementation (RS485, HTTP, etc.)
   */
  explicit InverterClient(ITransport& t): _t(t) {}

  /**
   * @brief Read holding registers from the inverter.
   * 
   * Performs a Modbus Function Code 0x03 (Read Holding Registers) operation
   * through the configured transport layer. The response format depends on
   * the transport used:
   * - Cloud/HTTP: Body string in TransportResult
   * - RS485: Raw bytes in TransportResult
   * 
   * @param slave Modbus slave address of the inverter (typically 1-247)
   * @param addr Starting register address to read from
   * @param qty Number of consecutive registers to read (1-125)
   * @return TransportResult containing the raw response data or error information
   */
  TransportResult readHolding(uint8_t slave, uint16_t addr, uint16_t qty);

private:
  ITransport& _t;  ///< Reference to the transport layer implementation
};
