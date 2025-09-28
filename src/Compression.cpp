#include "Compression.h"

// Compression to create separate records for each register value
std::vector<uint8_t> Compression::compressDelta(const std::vector<Record>& records) {
    std::vector<uint8_t> out;
    if (records.empty()) return out;
    
    Serial.printf("[COMPRESS] 🗜️ Starting delta compression of %d records\n", (int)records.size());
    
    // Count total number of individual register entries
    size_t total_regs = 0;
    for (const auto& r : records) {
        total_regs += r.regs.size();
    }
    
    out.push_back((uint8_t)total_regs);  // Total number of register records
    Serial.printf("[COMPRESS] Total registers to compress: %d\n", (int)total_regs);
    
    uint64_t prev_ts = records[0].ts_ms;
    uint64_t first_ts = records[0].ts_ms;
    uint64_t last_ts = records.back().ts_ms;
    
    Serial.printf("[COMPRESS] Timestamp range: %llu to %llu ms (span: %llu ms)\n", 
                 first_ts, last_ts, (last_ts - first_ts));
    
    for (const auto& r : records) {
        // Create a separate compressed record for each register in this Record
        for (const auto& reg : r.regs) {
            uint64_t delta = r.ts_ms - prev_ts;
            prev_ts = r.ts_ms;
            
            // Store delta (1 byte)
            out.push_back((uint8_t)delta);
            
            // Store raw register value (2 bytes, big-endian) 
            out.push_back((reg.raw >> 8) & 0xFF);
            out.push_back(reg.raw & 0xFF);
        }
    }
    
    Serial.printf("[COMPRESS]  Compressed to %d bytes (includes timestamps as deltas)\n", (int)out.size());
    return out;
}

std::vector<Record> Compression::decompressDelta(const std::vector<uint8_t>& data) {
    std::vector<Record> out;
    if (data.empty()) return out;
    
    size_t n = data[0];  // Total number of register records
    uint64_t ts = 0;
    size_t idx = 1;
    
    for (size_t i = 0; i < n && idx + 2 < data.size(); ++i) {
        uint8_t delta = data[idx++];
        ts += delta;
        uint16_t raw = (data[idx++] << 8) | data[idx++];
        
        Record r;
        r.ts_ms = ts;
        DecodedReg reg;
        reg.raw = raw;
        reg.value = raw; // Will be scaled by server
        r.regs.push_back(reg);
        out.push_back(r);
    }
    return out;
}
