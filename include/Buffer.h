#pragma once
#include <Arduino.h>
#include <vector>
#include "Transport.h"   // for DecodedReg

// One buffered sample/reading
struct Record {
  uint64_t ts_ms = 0;             // timestamp (millis or epoch ms)
  uint16_t start = 0;             // start address of the read
  uint16_t qty   = 0;             // number of registers
  String   rawFrameHex;           // optional: Modbus reply as hex (e.g., "110304...")
  std::vector<DecodedReg> regs;   // optional: decoded registers
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
