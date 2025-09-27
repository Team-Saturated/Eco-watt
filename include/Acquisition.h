#pragma once
#include <Arduino.h>
#include <vector>
#include "Buffer.h"        // Record, DecodedReg
#include "InverterClient.h"
#include "Modbus.h"


/**
 * @class Acquisition
 * @brief Performs Modbus/RS485 acquisitions from an inverter and decodes results into records.
 *
 */
class Acquisition {
public:

  /**
   * @brief Construct an Acquisition bound to an inverter client.
   * @param client Reference to a connected @c InverterClient used for I/O.
   */
  explicit Acquisition(InverterClient& client) : _client(client) {}

  /**
   * @brief Perform a single read and decode into a @c Record.
   *
   * Reads @p qty holding registers starting at @p start from Modbus slave @p slave.
   * On success, fills @p out and returns @c true. On failure, returns @c false and
   * writes a human-readable message into @p err.
   *
   * @param slave Modbus slave address (1–247).
   * @param start Starting register address.
   * @param qty   Number of consecutive registers to read.
   * @param[out] out Decoded record on success.
   * @param[out] err Error description on failure.
   * @return @c true on success, @c false on failure.
   */
  bool acquire(uint8_t slave, uint16_t start, uint16_t qty, Record& out, String& err);

private:
  /**
   * @brief Fallback decoder for Function Code 0x03 (Read Holding Registers) payloads.
   *
   * Interprets raw Modbus bytes @p rx as register values starting at @p startAddr,
   * covering @p qty registers, and appends decoded items to @p regs.
   *
   * @param rx Raw response frame payload (data section, not including Modbus header/CRC).
   * @param startAddr Starting register address corresponding to @p rx.
   * @param qty Number of registers expected in @p rx.
   * @param[out] regs Output vector populated with decoded register entries.
   * @param[out] err Error description on failure.
   * @return @c true if decoding succeeded, @c false otherwise.
   */
  bool decode03ToRegs(const std::vector<uint8_t>& rx,
                      uint16_t startAddr, uint16_t qty,
                      std::vector<DecodedReg>& regs, String& err);

  /**
   * @brief Provide scale factor and engineering unit for a register address.
   *
   * Sets @p scale and @p unit for known addresses; unknown addresses default to
   * scale = 1.0 and unit = "".
   *
   * @param addr Register address.
   * @param[out] scale Multiplicative factor to convert raw to engineering value.
   * @param[out] unit C-string of the engineering unit (e.g., "V", "A", "W").
   */
  void regScaleUnit(uint16_t addr, float& scale, const char*& unit) const;

  InverterClient& _client;
};
