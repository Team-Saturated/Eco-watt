#include "Poller.h"
#include "Transport.h"
#include "Config.h"
#include "Modbus.h"      
#include <vector>
#include "Mqtt.h"

static String toBase64_P(const uint8_t* data, size_t len) {
  size_t outLen = 0;
  (void) mbedtls_base64_encode(nullptr, 0, &outLen, data, len); // get size
  std::unique_ptr<uint8_t[]> out(new uint8_t[outLen + 1]);
  if (mbedtls_base64_encode(out.get(), outLen, &outLen, data, len) != 0) return String();
  out[outLen] = 0;
  return String((char*)out.get());
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

void Poller::read(uint8_t slave, uint16_t addr, uint16_t qty) {
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
    time_t hello;
    time(&hello); 
    uint32_t ts = (uint32_t)hello;
    Record rec;
    if (rec.buildFromRTU_Select_NoCRC(millis(), addr, res.bytes,REG_REQ_ID_1)) {
      bool kept = _buf.push(rec);   // record contains NO CRC; only [ts][qty][addr/data...]
      if (!kept) {
        Serial.println("[BUF] Warning: buffer full, oldest record dropped");
      }

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

void Poller::write(uint8_t slave, uint16_t addr, uint16_t value) {
  
  auto res = _c.writeSingle(slave, addr, value);

  std::vector<uint8_t> sealed;
  if (!sec.seal(/*type*/1, (const uint8_t*)res.error.c_str(),
                res.error.length(), sealed)) {
    Serial.println("[SEC] seal failed");
    
  }
  String mqttJson = toBase64_P(sealed.data(), sealed.size());
  if (res.ok) {
    Serial.print("Write works");
    Serial.println(res.error.c_str());
    mqttEnqueue(t_write_ack, (const uint8_t*)mqttJson.c_str(), false);

    }
  else {
    mqttEnqueue(t_write_ack, (const uint8_t*)mqttJson.c_str(), false);
  }
}

void Poller::changePeriod(uint32_t newPeriod) {
  if (newPeriod == 0) return; // ignore invalid
  _period = newPeriod;
  Serial.printf("[POLL] Changed polling period to %u ms\n", (unsigned)_period);
}