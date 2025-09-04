#include "Poller.h"
#include "Transport.h"
#include "Config.h"
#include "Modbus.h"      // for toHex helper if you have it
#include <vector>

// helper if you don’t have Modbus::toHex for arbitrary std::vector<uint8_t>
static String bytesToHex(const std::vector<uint8_t>& v) {
  String s;
  s.reserve(v.size() * 2);
  for (uint8_t b : v) {
    char buf[3];
    snprintf(buf, sizeof(buf), "%02X", b);
    s += buf;
  }
  return s;
}

void Poller::applyBackoff() {
  if (_backoffMs == 0) _backoffMs = BACKOFF_MIN_MS;
  else {
    uint32_t next = _backoffMs + BACKOFF_STEP_MS;
    _backoffMs = next > BACKOFF_MAX_MS ? BACKOFF_MAX_MS : next;
  }
  _next = millis() + _backoffMs;
}

void Poller::clearBackoff() {
  _consecErr = 0;
  _backoffMs = 0;
}

void Poller::loop(uint8_t slave, uint16_t addr, uint16_t qty) {
  const uint32_t now = millis();
  if (now < _next) return;

  auto res = _c.readHolding(slave, addr, qty);

  if (res.ok) {
    _consecOk++;
    if (_consecOk >= ERR_RESET_AFTER) {
      clearBackoff();
      _consecOk = 0;
    }

    // --- buffer the successful sample as a Record ---
    Record rec;
    rec.ts_ms = now;
    rec.start = addr;
    rec.qty   = qty;
    rec.regs  = res.regs;   // if CloudTransport decoded for us
    if (!res.bytes.empty()) {
      rec.rawFrameHex = bytesToHex(res.bytes);
    } else if (!res.body.isEmpty()) {
      // optional: keep raw JSON if you like
      rec.rawFrameHex = res.body; // or leave empty
    }

    const bool kept = _buf.push(rec);
    if (!kept) {
      // we overwrote oldest; optional log
      // Serial.println("[BUF] Dropped oldest record to make room");
    }

#if SIMULATE
    // concise immediate feedback (unchanged)
    if (!res.regs.empty()) {
      Serial.printf("[OK] %u regs from %u..%u\n",
                    (unsigned)res.regs.size(), addr, addr + qty - 1);
    } else if (!res.body.isEmpty()) {
      Serial.printf("[CLOUD OK] %s\n", res.body.c_str());
    } else if (!res.bytes.empty()) {
      Serial.print("[OK] ");
      for (auto b : res.bytes) Serial.printf("%02X", b);
      Serial.println();
    }
#else
    Serial.print("[RS485 OK] ");
    for (auto b: res.bytes) Serial.printf("%02X", b);
    Serial.println();
#endif

    // --- periodic flush/print of buffered Records ---
    if (now - _lastFlush >= _flushEveryMs) {
      std::vector<Record> out;
      _buf.drainTo(out);
      if (!out.empty()) {
        Serial.printf("[FLUSH] printing %u buffered sample(s)\n", (unsigned)out.size());
        for (const auto& r : out) {
          Serial.printf("  ts=%llu start=%u qty=%u\n",
                        (unsigned long long)r.ts_ms, r.start, r.qty);
          if (!r.rawFrameHex.isEmpty())
            Serial.printf("    raw=%s\n", r.rawFrameHex.c_str());
          for (const auto& reg : r.regs) {
            Serial.printf("    Addr=%u Raw=0x%04X -> %.3f %s\n",
                          reg.addr, reg.raw, reg.value, reg.unit.c_str());
          }
        }
      }
      _lastFlush = now;
    }

    // schedule next regular poll
    _next = now + _period;
    return;
  }

  // ---- Error path: act on error type ----
  _consecOk = 0;
  _consecErr++;

  Serial.printf("[ERR] type=%d status=%d msg=%s\n",
                (int)res.type, res.status, res.error.c_str());

  switch (res.type) {
    case ErrType::MODBUS_EXC:
      if (res.exc_code == 0x05 || res.exc_code == 0x06) {
        Serial.println("[ACT] Transient Modbus exception -> short backoff");
        applyBackoff();
      } else {
        Serial.println("[ACT] Hard Modbus exception -> regular backoff");
        applyBackoff();
      }
      break;

    case ErrType::TIMEOUT:
    case ErrType::HTTP:
    case ErrType::CRC:
    case ErrType::JSON:
    case ErrType::NO_DATA:
    case ErrType::OTHER:
    default:
      Serial.println("[ACT] Transport/format error -> backoff");
      applyBackoff();
      break;
  }
}
