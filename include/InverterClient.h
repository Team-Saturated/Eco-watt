#pragma once
#include "Transport.h"
#include <vector>

/**
 * @brief Client for communicating with inverter 
 * 
 * Provides high-level interface for reading and writing inverter registers
 * through various transport mechanisms (cloud, RS485)
 */
class InverterClient {
public:
  /**
   * @brief Constructs an InverterClient with the specified transport
   * @param t Reference to transport implementation
   */
  explicit InverterClient(ITransport& t): _t(t) {}
  
  /**
   * @brief Reads holding registers from inverter
   * @param slave Slave device address
   * @param addr Starting register address
   * @param qty Number of registers to read
   * @return TransportResult containing raw response (cloud: body string, RS485: bytes)
   */
  TransportResult readHolding(uint8_t slave, uint16_t addr, uint16_t qty);
  
  /**
   * @brief Writes a single register to inverter
   * @param slave Slave device address
   * @param addr Register address to write
   * @param value Value to write to register
   * @return TransportResult containing operation status and response
   */
  TransportResult writeSingle(uint8_t slave, uint16_t addr, uint16_t value);
  
private:
  ITransport& _t; ///< Reference to transport interface
};
