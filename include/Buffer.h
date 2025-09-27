#pragma once
#include <Arduino.h>
#include <vector>
#include "Transport.h"   // for DecodedReg

/**
 * @struct Record
 * @brief One buffered Modbus sample.
 *
 * A record may carry both the raw Modbus reply (hex string) and a parsed list
 * of decoded registers. The timestamp is in milliseconds (e.g., from millis().
 */

struct Record {
  uint64_t ts_ms = 0;           ///< Timestamp in milliseconds.
  uint16_t start = 0;           ///< Starting register address of the read.
  uint16_t qty   = 0;           ///< Number of consecutive registers read.
  String   rawFrameHex;         ///< Optional: reply as hex (e.g., "110304...").
  std::vector<DecodedReg> regs; ///< Optional: decoded register list.
};

/**
 * @class RingBuffer
 * @brief Fixed-capacity FIFO ring buffer for @ref Record objects.
 *
 * The buffer overwrites the oldest items when full (drop-on-overflow policy).
 * Use @ref droppedCount to track how many items were overwritten.
 */
class RingBuffer {
public:
   /**
   * @brief Construct a ring buffer.
   * @param capacity Maximum number of records retained.
   */
  explicit RingBuffer(size_t capacity);
  
  /// Default destructor.
  ~RingBuffer() = default;

  /**
   * @brief Push a record into the buffer.
   * @param r The record to push (copied).
   * @return `true` if appended without overwrite; `false` if the buffer was full
   *         and the oldest item was dropped to make room.
   * @note When `false` is returned, @ref droppedCount increases by 1.
   */
  bool push(const Record& r);

  /**
   * @brief Move all buffered items into @p out and clear the buffer.
   * @param[out] out Destination vector; items are appended (moved) to it.
   *
   * After this call, the buffer is empty (`size() == 0`).
   */
  void drainTo(std::vector<Record>& out);

  /// Remove all items; does not reset @ref droppedCount.
  void clear();

  /// Stats / introspection
  size_t size() const { return _size; }
  size_t capacity() const { return _cap; }
  bool   empty() const { return _size == 0; }
  uint32_t droppedCount() const { return _dropped; }

private:
  size_t _cap;                 ///< Fixed capacity (number of records).
  size_t _head;                ///< Next write index.
  size_t _tail;                ///< Next read index.
  size_t _size;                ///< Current item count.
  uint32_t _dropped;           ///< Cumulative overwritten count.
  std::vector<Record> _buf;    ///< Storage for records.
};
