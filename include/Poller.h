#pragma once
#include <Arduino.h>
#include "InverterClient.h"
#include "Buffer.h"     // <-- your RingBuffer/Record

/**
 * @class Poller
 * @brief Manages periodic polling of inverter registers with error handling and backoff.
 * 
 * The Poller class handles reading from and writing to inverter registers at specified
 * intervals. It includes adaptive backoff mechanisms for error handling and manages
 * buffering of polled data.
 */
class Poller {
public:
  /**
   * @brief Constructs a new Poller object.
   * 
   * @param c Reference to InverterClient for communication with the inverter
   * @param periodMs Polling period in milliseconds
   * @param buf Reference to RingBuffer for storing polled data
   */
  Poller(InverterClient& c, uint32_t periodMs, RingBuffer& buf)
  : _c(c), _period(periodMs), _buf(buf) {}

  /**
   * @brief Reads data from inverter holding registers.
   * 
   * @param slave Modbus slave address
   * @param addr Starting register address
   * @param qty Number of registers to read
   */
  void read(uint8_t slave, uint16_t addr, uint16_t qty);
  
  /**
   * @brief Writes a value to an inverter holding register.
   * 
   * @param slave Modbus slave address
   * @param addr Register address to write to
   * @param value Value to write
   */
  void write(uint8_t slave, uint16_t addr, uint16_t value);
  
  /**
   * @brief Changes the polling period.
   * 
   * @param newPeriod New polling period in milliseconds
   */
  void changePeriod(uint32_t newPeriod);

private:
  InverterClient& _c;           ///< Reference to the inverter client for Modbus communication
  uint32_t _period;             ///< Polling period in milliseconds
  uint32_t _next = 0;           ///< Timestamp for the next scheduled poll

  // error/backoff tracking
  uint32_t _backoffMs = 0;      ///< Current backoff delay in milliseconds
  uint8_t  _consecErr = 0;      ///< Count of consecutive errors
  uint8_t  _consecOk  = 0;      ///< Count of consecutive successful operations

  RingBuffer& _buf;             ///< Reference to external ring buffer for data storage
  uint32_t   _lastFlush = 0;    ///< Timestamp of last buffer flush operation
  const uint32_t _flushEveryMs = 15000;  ///< Buffer flush interval (15 seconds)

  /**
   * @brief Applies exponential backoff delay after communication errors.
   * 
   * Increases the backoff delay based on consecutive error count to reduce
   * communication attempts when the inverter is unresponsive.
   */
  void applyBackoff();
  
  /**
   * @brief Clears the backoff delay after successful communications.
   * 
   * Resets error counters and backoff delays when communication is restored.
   */
  void clearBackoff();
};
