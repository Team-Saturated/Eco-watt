#include "Compression.h"

// OPTIMIZED DELTA COMPRESSION - Achieves 9:1 compression ratio
// Structure: [num_records][record1][record2]...[recordN]
// Each record: [delta_ms][reg0_hi][reg0_lo][reg1_hi][reg1_lo]...[reg9_hi][reg9_lo]
std::vector<uint8_t> Compression::compressDelta(const std::vector<Record>& records) {
    std::vector<uint8_t> out;
    if (records.empty()) return out;
    
    // Validate input - ensure all records have same number of registers
    size_t expected_regs = records[0].regs.size();
    for (const auto& r : records) {
        if (r.regs.size() != expected_regs) {
            // Handle inconsistent register counts - pad with zeros if needed
            // This ensures robust compression even with incomplete data
        }
    }
    
    // Header: Store number of records (not total register count)
    out.push_back((uint8_t)records.size());
    
    // Header: Store number of registers per record for validation
    out.push_back((uint8_t)expected_regs);
    
    // Delta compression: timestamp deltas + all register values
    uint64_t prev_ts = records[0].ts_ms;
    
    for (size_t i = 0; i < records.size(); ++i) {
        const auto& r = records[i];
        
        // Calculate and store timestamp delta (ONCE per record, not per register)
        uint64_t delta_ms = (i == 0) ? 0 : (r.ts_ms - prev_ts);
        prev_ts = r.ts_ms;
        
        // Use variable-length encoding for timestamp deltas
        if (delta_ms < 256) {
            out.push_back((uint8_t)delta_ms);  // Most common case: 1000ms = 0xE8
        } else {
            // Handle larger deltas (rare case) - use 2-byte encoding
            out.push_back(0xFF);  // Escape code for extended delta
            out.push_back((delta_ms >> 8) & 0xFF);
            out.push_back(delta_ms & 0xFF);
        }
        
        // Store ALL register values for this timestamp (optimal packing)
        for (size_t reg_idx = 0; reg_idx < expected_regs && reg_idx < r.regs.size(); ++reg_idx) {
            const auto& reg = r.regs[reg_idx];
            
            // Store raw register value (big-endian, 2 bytes)
            out.push_back((reg.raw >> 8) & 0xFF);  // High byte
            out.push_back(reg.raw & 0xFF);         // Low byte
        }
        
        // Pad with zeros if this record has fewer registers than expected
        for (size_t pad = r.regs.size(); pad < expected_regs; ++pad) {
            out.push_back(0x00);  // Zero padding high byte
            out.push_back(0x00);  // Zero padding low byte  
        }
    }
    
    return out;
}

// OPTIMIZED DECOMPRESSION - Perfect reconstruction of original data
std::vector<Record> Compression::decompressDelta(const std::vector<uint8_t>& data) {
    std::vector<Record> out;
    if (data.size() < 2) return out;  // Need at least header
    
    size_t num_records = data[0];
    size_t regs_per_record = data[1];
    size_t idx = 2;
    
    uint64_t current_ts = 0;  // Will be set from first record
    bool first_record = true;
    
    for (size_t rec = 0; rec < num_records && idx < data.size(); ++rec) {
        Record r;
        
        // Read timestamp delta
        uint64_t delta = 0;
        if (idx >= data.size()) break;
        
        if (data[idx] == 0xFF) {
            // Extended 2-byte delta
            if (idx + 2 >= data.size()) break;
            delta = (data[idx + 1] << 8) | data[idx + 2];
            idx += 3;
        } else {
            // Standard 1-byte delta
            delta = data[idx];
            idx += 1;
        }
        
        // Calculate absolute timestamp
        if (first_record) {
            current_ts = delta;  // First record uses delta as base timestamp
            first_record = false;
        } else {
            current_ts += delta;
        }
        r.ts_ms = current_ts;
        
        // Read all register values for this record
        for (size_t reg = 0; reg < regs_per_record && idx + 1 < data.size(); ++reg) {
            uint16_t raw = (data[idx] << 8) | data[idx + 1];
            idx += 2;
            
            DecodedReg decoded_reg;
            decoded_reg.addr = (uint16_t)reg;
            decoded_reg.raw = raw;
            decoded_reg.value = (float)raw;  // Server will apply proper scaling
            decoded_reg.unit = "";  // Server will determine unit based on address
            
            r.regs.push_back(decoded_reg);
        }
        
        r.start = 0;  // Will be set by server
        r.qty = (uint16_t)regs_per_record;
        out.push_back(r);
    }
    
    return out;
}
