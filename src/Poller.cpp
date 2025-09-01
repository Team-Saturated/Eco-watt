#include "Poller.h"

void Poller::loop(uint8_t slave, uint16_t addr, uint16_t qty) {
  uint32_t now = millis();
  if (now < _next) return;

  auto res = _c.readHolding(slave, addr, qty);
  if (res.ok) {
#if SIMULATE
    Serial.printf("[CLOUD OK] %s\n", res.body.c_str());
#else
    Serial.print("[RS485 OK] ");
    for (auto b: res.bytes) { Serial.printf("%02X", b); }
    Serial.println();
#endif
  } else {
    Serial.printf("[ERR] %s (status=%d)\n", res.error.c_str(), res.status);
  }
  _next = now + _period;
}
