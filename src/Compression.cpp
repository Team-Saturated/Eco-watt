#include "Compression.h"
#include <vector>
#include <cstdint>
#include <cstring>
#include <Arduino.h>   

static inline uint16_t popcount16(uint16_t x) {
  // portable bit count
  x = x - ((x >> 1) & 0x5555);
  x = (x & 0x3333) + ((x >> 2) & 0x3333);
  return uint16_t((((x + (x >> 4)) & 0x0F0F) * 0x0101) >> 8);
}

static inline void put16_le(std::vector<uint8_t>& v, uint16_t x){
  v.push_back(uint8_t(x)); v.push_back(uint8_t(x>>8));
}
static inline void put32_le(std::vector<uint8_t>& v, uint32_t x){
  v.push_back(uint8_t(x)); v.push_back(uint8_t(x>>8));
  v.push_back(uint8_t(x>>16)); v.push_back(uint8_t(x>>24));
}
static inline void put64_le(std::vector<uint8_t>& v, uint64_t x){
  for (int i=0;i<8;++i) v.push_back(uint8_t(x>>(8*i)));
}
static inline bool get16_le(const std::vector<uint8_t>& v, size_t& i, uint16_t& x){
  if (i + 2 > v.size()) return false;
  x = uint16_t(v[i]) | (uint16_t(v[i+1]) << 8); i += 2; return true;
}
static inline bool get32_le(const std::vector<uint8_t>& v, size_t& i, uint32_t& x){
  if (i + 4 > v.size()) return false;
  x = uint32_t(v[i]) | (uint32_t(v[i+1]) << 8) | (uint32_t(v[i+2]) << 16) | (uint32_t(v[i+3]) << 24);
  i += 4; return true;
}
static inline bool get64_le(const std::vector<uint8_t>& v, size_t& i, uint64_t& x){
  if (i + 8 > v.size()) return false;
  x = 0; for (int b=0;b<8;++b) x |= (uint64_t(v[i+b]) << (8*b)); i += 8; return true;
}

static inline uint32_t zigzag32(int32_t v) { return (uint32_t)((v << 1) ^ (v >> 31)); }
static inline int32_t  unzigzag32(uint32_t u){ return (int32_t)((u >> 1) ^ -int32_t(u & 1)); }

static inline void putVarUint32(std::vector<uint8_t>& out, uint32_t v) {
  while (v >= 0x80) { out.push_back(uint8_t(v) | 0x80); v >>= 7; }
  out.push_back(uint8_t(v));
}

static inline bool getVarUint32(const std::vector<uint8_t>& in, size_t& i, uint32_t& v) {
  v = 0; uint32_t shift = 0;
  while (i < in.size()) {
    uint8_t b = in[i++];
    v |= uint32_t(b & 0x7F) << shift;
    if (!(b & 0x80)) return true;
    shift += 7;
    if (shift > 28) break; // guard
  }
  return false;
}


// Input records expect r.raw = [(addr u16)(data u16)]*qty, with addr in 0..9
// Wire format: [base_ts u64][nrecs u16] then per rec: [dt u32][mask u16][varint(deltas for set bits)]
std::vector<uint8_t> Compression::compressDelta(const std::vector<Record>& records) {
  std::vector<uint8_t> out;
  if (records.empty()) return out;

  // -------- First pass: count valid recs & compute "naive/original" input size --------
  // Baseline "in_size" = size we would have sent without compression (addresses included):
  //  [base_ts:8][nrecs:2] + sum{ [dt:4][qty:2] + raw.size() }
  size_t in_size = 8 /*base_ts*/ + 2 /*nrecs*/;
  uint16_t n_valid = 0;
  for (auto const& r : records) {
    if (r.raw.size() != size_t(r.qty) * 4) continue; // skip invalid
    in_size += 4 /*dt*/ + 2 /*qty*/ + r.raw.size();
    ++n_valid;
  }
  if (n_valid == 0) return out;

  // -------- Header --------
  put64_le(out, records.front().ts_ms);
  put16_le(out, n_valid); // exact count of valid recs

  // Last known values for regs 0..9
  uint16_t last[10] = {0};

  uint64_t prev_ts = records.front().ts_ms;

  // -------- Second pass: emit compressed --------
  for (auto const& r : records) {
    if (r.raw.size() != size_t(r.qty) * 4) continue;

    // Build a snapshot by addr from r.raw; default to previous last[]
    uint16_t curr[10];
    for (int k=0;k<10;++k) curr[k] = last[k];

    const uint8_t* p = r.raw.data();
    for (uint16_t i = 0; i < r.qty; ++i) {
      uint16_t addr = uint16_t(p[0] | (p[1] << 8));
      uint16_t data = uint16_t(p[2] | (p[3] << 8));
      p += 4;
      if (addr < 10) curr[addr] = data; // ignore out-of-range addresses
    }

    // Timestamp delta
    uint32_t dt = (r.ts_ms >= prev_ts) ? uint32_t(r.ts_ms - prev_ts) : 0;
    prev_ts = r.ts_ms;
    putVarUint32(out, dt); 

    // Mask (only bits 0..9 used)
    uint16_t mask = (REG_REQ_ID_1 & 0x03FF);
    
    put16_le(out, mask);

    // Deltas for set bits in ascending reg index
    for (uint8_t reg = 0; reg < 10; ++reg) {
      if (!(mask & (1u << reg))) continue;

      int32_t delta = int32_t(int16_t(curr[reg])) - int32_t(int16_t(last[reg]));
      uint32_t enc  = zigzag32(delta);
      putVarUint32(out, enc);

      last[reg] = curr[reg]; // update state
    }
  }

  // -------- Print size comparison --------
  size_t out_size = out.size();
  long long saved = (long long)in_size - (long long)out_size;
  double ratio_pct = (in_size > 0) ? (100.0 * double(out_size) / double(in_size)) : 0.0;
  double factor    = (out_size > 0) ? (double(in_size) / double(out_size)) : 0.0;

  Serial.printf("[Compression] in=%zu bytes, out=%zu bytes, saved=%lld bytes, ratio=%.2f%% (%.2fx)\n",
                in_size, out_size, saved, ratio_pct, factor);

  return out;
}


// with raw = [(addr u16)(data u16)]*qty (LE). ts is reconstructed from base+dt chain.
std::vector<Record> Compression::decompressDelta(const std::vector<uint8_t>& in) {
  std::vector<Record> out;
  size_t i = 0;

  uint64_t base_ts = 0;
  uint16_t nrecs   = 0;
  if (!get64_le(in, i, base_ts)) return {};
  if (!get16_le(in, i, nrecs))   return {};

  out.reserve(nrecs);

  // Last known values for regs 0..9
  uint16_t last[10] = {0};

  uint64_t ts = base_ts;

  for (uint16_t r = 0; r < nrecs; ++r) {
    uint32_t dt = 0; uint16_t mask = 0;
    if (!get32_le(in, i, dt))   return {};
    if (!get16_le(in, i, mask)) return {};
    mask &= 0x03FF; // only 0..9

    ts += dt;

    // Start with last[]; we only emit regs whose bit is set
    uint16_t curr[10];
    for (int k=0;k<10;++k) curr[k] = last[k];

    // Read varint deltas for each set bit (ascending reg index)
    for (uint8_t reg = 0; reg < 10; ++reg) {
      if (!(mask & (1u << reg))) continue;
      uint32_t enc = 0;
      if (!getVarUint32(in, i, enc)) return {};
      int32_t delta = unzigzag32(enc);
      int32_t val   = int32_t(int16_t(last[reg])) + delta;
      if (val < 0) val = 0; else if (val > 0xFFFF) val = 0xFFFF; // clamp if unsigned
      curr[reg] = (uint16_t)val;
      last[reg] = (uint16_t)val;
    }

    // Rebuild legacy layout: [(addr u16)(data u16)]*qty for regs in mask
    Record rec;
    rec.ts_ms = ts;
    rec.raw.clear();
    uint16_t qty = popcount16(mask);
    rec.raw.reserve(size_t(qty) * 4);

    for (uint8_t reg = 0; reg < 10; ++reg) {
      if (!(mask & (1u << reg))) continue;
      // addr (LE)
      rec.raw.push_back(uint8_t(reg & 0xFF));
      rec.raw.push_back(uint8_t(reg >> 8));
      // data (LE)
      uint16_t v = curr[reg];
      rec.raw.push_back(uint8_t(v & 0xFF));
      rec.raw.push_back(uint8_t(v >> 8));
    }
    rec.qty = qty;

    out.emplace_back(std::move(rec));
  }
  return out;
}
