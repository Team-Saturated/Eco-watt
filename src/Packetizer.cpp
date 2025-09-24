#include "Packetizer.h"

// Finalize a compressed block for upload
std::vector<uint8_t> Packetizer::finalizeBlock(const std::vector<Record>& records) {
    // Compress using delta encoding
    std::vector<uint8_t> compressed = Compression::compressDelta(records);
    return compressed;
}

// Simulate encryption/HMAC (stub)
std::vector<uint8_t> Packetizer::encryptAndMac(const std::vector<uint8_t>& data) {
    // Placeholder: just append a fake MAC (4 bytes)
    std::vector<uint8_t> out = data;
    out.push_back(0xDE); out.push_back(0xAD); out.push_back(0xBE); out.push_back(0xEF);
    return out;
}

// Chunk data for unreliable links
std::vector<std::vector<uint8_t>> Packetizer::chunkData(const std::vector<uint8_t>& data, size_t chunkSize) {
    std::vector<std::vector<uint8_t>> chunks;
    size_t total = data.size();
    for (size_t i = 0; i < total; i += chunkSize) {
        size_t end = (i + chunkSize < total) ? (i + chunkSize) : total;
        chunks.emplace_back(data.begin() + i, data.begin() + end);
    }
    return chunks;
}
