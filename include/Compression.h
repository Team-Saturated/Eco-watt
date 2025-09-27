#pragma once
#include <Arduino.h>
#include <vector>
#include "Buffer.h"

// Lightweight delta compression for time-series data
class Compression {
    public:
        // Compress a vector of Records using delta encoding
        static std::vector<uint8_t> compressDelta(const std::vector<Record>& records);

        // Decompress a vector of bytes back to Records
        static std::vector<Record> decompressDelta(const std::vector<uint8_t>& data);
    };
