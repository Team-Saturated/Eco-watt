#include "Compression.h"

// Compression to create separate records for each register value

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

std::vector<uint8_t> Compression::compressDelta(const std::vector<Record>& records) {
  std::vector<uint8_t> out;
  if (records.empty()) return out;

  auto const& recs = records;

  // Header
  put64_le(out, recs.front().ts_ms);               // base_ts
  put16_le(out, static_cast<uint16_t>(recs.size())); // nrecs (assume <= 65535)

  uint64_t prev_ts = recs.front().ts_ms;
  for (auto const& r : recs) {
    // sanity
    if (r.raw.size() != size_t(r.qty) * 4) continue;

    // dt as u32 (no clamping)
    uint64_t d64 = (r.ts_ms >= prev_ts) ? (r.ts_ms - prev_ts) : 0;
    prev_ts = r.ts_ms;
    put32_le(out, static_cast<uint32_t>(d64));     // dt_ms

    // qty and payload
    put16_le(out, r.qty);
    out.insert(out.end(), r.raw.begin(), r.raw.end()); // [(addr)(data)]*qty (LE)
  }

  return out;
}


// ---- helpers (LE) ----
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

// Format:
// [base_ts:u64][nrecs:u16] then per record:
//   [dt_ms:u32][qty:u16][(addr:u16)(data:u16)]*qty
std::vector<Record> Compression::decompressDelta(const std::vector<uint8_t>& in) {
  std::vector<Record> out;
  size_t i = 0;

  uint64_t base_ts = 0;
  uint16_t nrecs   = 0;
  if (!get64_le(in, i, base_ts)) return {};
  if (!get16_le(in, i, nrecs))   return {};

  out.reserve(nrecs);
  uint64_t prev_ts = base_ts;

  for (uint16_t r = 0; r < nrecs; ++r) {
    uint32_t dt_ms = 0;
    uint16_t qty   = 0;
    if (!get32_le(in, i, dt_ms)) return {};
    if (!get16_le(in, i, qty))   return {};

    const size_t need = size_t(qty) * 4;
    if (i + need > in.size())    return {};

    Record rec;
    rec.ts_ms = prev_ts + uint64_t(dt_ms);
    rec.qty   = qty;
    rec.raw.assign(in.begin() + i, in.begin() + i + need);
    i += need;

    // (optional) sanity
    if (rec.raw.size() != size_t(rec.qty) * 4) return {};

    out.emplace_back(std::move(rec));
    prev_ts = out.back().ts_ms;
  }

  // (optional) ensure no trailing bytes: if (i != in.size()) return {};
  return out;
}

