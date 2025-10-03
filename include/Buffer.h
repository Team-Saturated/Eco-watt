#pragma once
#include <Arduino.h>
#include <vector>
#include "Transport.h"   // for DecodedReg

// Stored layout: [ts:u64][qty:u16] and then raw = [(addr:u16)(data:u16)] * qty
struct Record {
  uint64_t ts_ms{0};
  uint16_t qty{0};
  std::vector<uint8_t> raw; // [(addr LE)(data LE)] * qty

  // helpers (LE out, BE in)
  static inline void put16(std::vector<uint8_t>& v, uint16_t x) {
    v.push_back(uint8_t(x & 0xFF));
    v.push_back(uint8_t(x >> 8));
  }
  static inline uint16_t be16(const uint8_t* p) { // Modbus data is BE
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

// Fixed-size ring buffer for Record
class RingBuffer {
public:
  explicit RingBuffer(size_t capacity);
  ~RingBuffer() = default;

  // Push a record (returns false if an old item was dropped to make room)
  bool push(const Record& r);

  // Move all items into 'out' (out is appended). Buffer becomes empty.
  void drainTo(std::vector<Record>& out);

  // Clear everything
  void clear();

  // Stats / introspection
  size_t size() const { return _size; }
  size_t capacity() const { return _cap; }
  bool   empty() const { return _size == 0; }
  uint32_t droppedCount() const { return _dropped; }

private:
  size_t _cap;
  size_t _head;       // next write index
  size_t _tail;       // next read index
  size_t _size;       // current number of items
  uint32_t _dropped;  // number of overwritten/dropped items
  std::vector<Record> _buf;
};
