#pragma once
#include <Arduino.h>
#include <vector>
#include "Buffer.h"
#include "Compression.h"

// Packetizer: prepares compressed blocks for upload, handles chunking/retry, and stubs encryption
class Packetizer {
public:
    // Finalize a compressed block for upload
    static std::vector<uint8_t> finalizeBlock(const std::vector<Record>& records);

    // Simulate encryption/HMAC (stub)
    static std::vector<uint8_t> encryptAndMac(const std::vector<uint8_t>& data);

    // Chunk data for unreliable links
    static std::vector<std::vector<uint8_t>> chunkData(const std::vector<uint8_t>& data, size_t chunkSize);
};
