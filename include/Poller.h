/**
 * @file Poller.h
 * @brief Periodic data acquisition controller for solar inverter monitoring.
 * 
 * This file defines the Poller class which manages periodic polling of inverter
 * data, error handling with exponential backoff, and automatic buffer flushing.
 */

#pragma once
#include <Arduino.h>
#include "InverterClient.h"
#include "Buffer.h"

/**
 * @class Poller
 * @brief Manages periodic polling of inverter data with error handling and buffering.
 * 
 * The Poller class orchestrates regular data acquisition from solar inverters,
 * implementing robust error handling with exponential backoff for failed requests
 * and automatic buffer management with periodic flushing.
 */
class Poller {
public:
  /**
   * @brief Construct a new Poller object.
   * 
   * @param c Reference to the InverterClient used for communication
   * @param periodMs Polling period in milliseconds between data acquisition attempts
   * @param buf Reference to the RingBuffer for storing acquired data records
   */
  Poller(InverterClient& c, uint32_t periodMs, RingBuffer& buf)
  : _c(c), _period(periodMs), _buf(buf) {}

  /**
   * @brief Main polling loop function to be called repeatedly.
   * 
   * This function should be called continuously in the main loop. It manages
   * timing for periodic data acquisition, handles errors with backoff logic,
   * and performs automatic buffer flushing.
   * 
   * @param slave Modbus slave address of the inverter
   * @param addr Starting register address to read from
   * @param qty Number of consecutive registers to read
   */
  void loop(uint8_t slave, uint16_t addr, uint16_t qty);

private:
  InverterClient& _c;        ///< Reference to the inverter client for communication
  uint32_t _period;          ///< Polling period in milliseconds
  uint32_t _next = 0;        ///< Timestamp for next scheduled poll

  // Error and backoff tracking
  uint32_t _backoffMs = 0;   ///< Current backoff delay in milliseconds
  uint8_t  _consecErr = 0;   ///< Count of consecutive errors
  uint8_t  _consecOk  = 0;   ///< Count of consecutive successful operations

  RingBuffer& _buf;                        ///< Reference to data buffer
  uint32_t   _lastFlush = 0;              ///< Timestamp of last buffer flush
  const uint32_t _flushEveryMs = 15000;   ///< Buffer flush interval (15 seconds)

  /**
   * @brief Apply exponential backoff after communication errors.
   * 
   * Increases the backoff delay for the next polling attempt based on
   * the number of consecutive errors encountered.
   */
  void applyBackoff();

  /**
   * @brief Clear backoff state after successful communication.
   * 
   * Resets the error tracking and backoff delay after sufficient
   * consecutive successful operations.
   */
  void clearBackoff();
};
