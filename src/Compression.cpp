#include "Compression.h"

// Example: compress only timestamps and one value (extend as needed)
std::vector<uint8_t> Compression::compressDelta(const std::vector<Record>& records) {
    std::vector<uint8_t> out;
    if (records.empty()) return out;
    uint64_t prev_ts = records[0].ts_ms;
    out.push_back((uint8_t)records.size());
    for (const auto& r : records) {
        uint64_t delta = r.ts_ms - prev_ts;
        prev_ts = r.ts_ms;
        // Store delta (1 byte, for demo; use varint for real)
        out.push_back((uint8_t)delta);
        // Store first register value (demo)
        if (!r.regs.empty()) {
            float v = r.regs[0].value;
            uint16_t vi = (uint16_t)(v * 100); // scale for demo
            out.push_back((vi >> 8) & 0xFF);
            out.push_back(vi & 0xFF);
        } else {
            out.push_back(0); out.push_back(0);
        }
    }
    return out;
}

std::vector<Record> Compression::decompressDelta(const std::vector<uint8_t>& data) {
    std::vector<Record> out;
    if (data.empty()) return out;
    size_t n = data[0];
    uint64_t ts = 0;
    size_t idx = 1;
    for (size_t i = 0; i < n && idx + 2 < data.size(); ++i) {
        uint8_t delta = data[idx++];
        ts += delta;
        uint16_t vi = (data[idx++] << 8) | data[idx++];
        float v = vi / 100.0f;
        Record r;
        r.ts_ms = ts;
        DecodedReg reg;
        reg.value = v;
        r.regs.push_back(reg);
        out.push_back(r);
    }
    return out;
}
