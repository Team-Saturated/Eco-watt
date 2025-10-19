#pragma once
#include <Arduino.h>
#include <vector>
#include "Buffer.h"

/**
 * @brief Structure representing a decoded record with timestamp and sensor values
 * 
 * This structure holds decompressed time-series data with a timestamp and up to 10
 * sensor values. The mask field indicates which values are valid.
 */
struct DecodedRec {
  uint64_t ts_ms;           ///< Timestamp in milliseconds
  uint16_t values[10];      ///< Array of sensor values (0-9). Only values indicated by mask are valid
  uint16_t mask;            ///< Bitmask indicating which values are valid (bit N set = values[N] is valid)
};

/**
 * @brief Lightweight delta compression for time-series data
 * 
 * This class provides static methods for compressing and decompressing time-series
 * sensor data using delta encoding to reduce storage and transmission overhead.
 */
class Compression {
public:
    /**
     * @brief Compress a vector of Records using delta encoding
     * 
     * Applies delta compression to reduce the size of time-series data by storing
     * differences between consecutive values rather than absolute values.
     * 
     * @param records Vector of Record objects to compress
     * @return std::vector<uint8_t> Compressed data as a byte vector
     */
    static std::vector<uint8_t> compressDelta(const std::vector<Record>& records);

    /**
     * @brief Decompress a vector of bytes back to Records
     * 
     * Reverses the delta compression to reconstruct the original Record objects
     * from the compressed byte stream.
     * 
     * @param data Compressed data as a byte vector
     * @return std::vector<Record> Decompressed vector of Record objects
     */
    static std::vector<Record> decompressDelta(const std::vector<uint8_t>& data);
};
