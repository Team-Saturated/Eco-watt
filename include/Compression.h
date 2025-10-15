#pragma once
#include <Arduino.h>
#include <vector>
#include "Buffer.h"

struct DecodedRec {
  uint64_t ts_ms;
  // values[0..9] valid for bits set in mask; others unchanged
  uint16_t values[10];
  uint16_t mask;
};
// Lightweight delta compression for time-series data
class Compression {
public:
    // Compress a vector of Records using delta encoding
    static std::vector<uint8_t> compressDelta(const std::vector<Record>& records);

    // Decompress a vector of bytes back to Records
    static std::vector<Record> decompressDelta(const std::vector<uint8_t>& data);
};
