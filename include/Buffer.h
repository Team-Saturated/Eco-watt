#pragma once
#include <Arduino.h>
#include <vector>
#include "Transport.h"   

/**
 * @brief Record structure for storing Modbus register data with timestamp
 * 
 * Storage layout: [timestamp:uint64][quantity:uint16][raw_data]
 * Raw data format: [(address:uint16)(data:uint16)] * quantity (little-endian)
 */
struct Record {
  uint64_t ts_ms{0};              ///< Timestamp in milliseconds
  uint16_t qty{0};                ///< Number of register pairs stored
  std::vector<uint8_t> raw;       ///< Raw data buffer: [(addr LE)(data LE)] * qty

  /**
   * @brief Write a 16-bit value to vector in little-endian format (BE->LE)
   * @param v Target vector to append bytes
   * @param x 16-bit value to write
   */
  static inline void put16(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back(uint8_t(x & 0xFF));
    v.push_back(uint8_t(x >> 8));
  }

  /**
   * @brief Read a 16-bit value from buffer in big-endian format (Modbus)
   * @param p Pointer to 2-byte buffer
   * @return 16-bit value
   */
  static inline uint16_t be16(const uint8_t* p) {
    return (uint16_t(p[0]) << 8) | p[1];
  }

  // frame: [slave][func][bytecount][data...][(crc_lo)(crc_hi)]  (CRC ignored)
  // Bit 0 => start_addr + 0, bit 1 => start_addr + 1, ...
  bool buildFromRTU_Select_NoCRC(uint64_t ts,
                                 uint16_t start_addr,
                                 const std::vector<uint8_t>& frame,
                                 uint16_t select_mask) {
    if (frame.size() < 3) return false;

    const uint8_t bc = frame[2];
    if ((bc & 1) != 0) return false;              // must be whole 16-bit regs

    const size_t data_start = 3;
    const size_t data_end   = data_start + bc;    // exclusive
    if (frame.size() < data_end) return false;    // ensure payload exists

    const uint8_t* p = frame.data() + data_start;
    const uint16_t q = bc / 2;                    // total regs returned

    ts_ms = ts;

    // Ignore selector bits beyond q registers
    if (q < 16) {
      const uint16_t keep_mask = (q == 16) ? 0xFFFFu : ((1u << q) - 1u);
      select_mask &= keep_mask;
    }

    // Pre-count selected pairs for reserve()
    uint16_t qty_sel = 0;
    for (uint16_t i = 0; i < q; ++i) {
      if (select_mask & (1u << i)) ++qty_sel;
    }

    qty = qty_sel;
    raw.clear();
    raw.reserve(size_t(qty_sel) * 4);

    // Emit only selected (addr,data) pairs
    for (uint16_t i = 0; i < q; ++i) {
      if (select_mask & (1u << i)) {
        const uint16_t reg_addr = uint16_t(start_addr + i);
        const uint16_t reg_val  = be16(p + (i * 2)); // read BE from RTU
        put16(raw, reg_addr);                        // write LE
        put16(raw, reg_val);                         // write LE
      }
    }

    // If you want to drop empty records, return (qty > 0);
    return true;
  }
};

/**
 * @brief Fixed-size circular buffer for storing Record objects
 * 
 * Ring buffer that overwrites oldest entries when full.
 * Tracks the number of dropped records due to capacity constraints.
 */
class RingBuffer {
public:
  /**
   * @brief Construct a ring buffer with specified capacity
   * @param capacity Maximum number of Record objects to store
   */
  explicit RingBuffer(size_t capacity);

  ~RingBuffer() = default;

  /**
   * @brief Add a record to the buffer
   * 
   * If buffer is full, the oldest record is overwritten and drop counter is incremented.
   * 
   * @param r Record to store
   * @return false if an old item was dropped to make room, true otherwise
   */
  bool push(const Record& r);

  /**
   * @brief Move all stored records to output vector and clear buffer
   * 
   * Records are appended to the output vector. Buffer becomes empty after this call.
   * 
   * @param out Destination vector (records are appended)
   */
  void drainTo(std::vector<Record>& out);

  /**
   * @brief Remove all records from the buffer
   */
  void clear();

  /**
   * @brief Get the current number of records stored
   * @return Number of records in buffer
   */
  size_t size() const { return _size; }

  /**
   * @brief Get the maximum capacity of the buffer
   * @return Maximum number of records
   */
  size_t capacity() const { return _cap; }

  /**
   * @brief Check if buffer is empty
   * @return true if no records are stored
   */
  bool   empty() const { return _size == 0; }

  /**
   * @brief Get the total count of dropped records since creation
   * @return Number of records overwritten due to capacity limits
   */
  uint32_t droppedCount() const { return _dropped; }

private:
  size_t _cap;                    ///< Maximum buffer capacity
  size_t _head;                   ///< Next write index
  size_t _tail;                   ///< Next read index
  size_t _size;                   ///< Current number of items in buffer
  uint32_t _dropped;              ///< Count of overwritten/dropped items
  std::vector<Record> _buf;       ///< Internal storage
};
