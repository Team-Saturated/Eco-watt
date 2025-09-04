#pragma once
#include <Arduino.h>
#include "InverterClient.h"
#include "Buffer.h"     // <-- your RingBuffer/Record

class Poller {
public:
  Poller(InverterClient& c, uint32_t periodMs)
  : _c(c), _period(periodMs), _buf(64) {}   // capacity 64

  void loop(uint8_t slave, uint16_t addr, uint16_t qty);

private:
  InverterClient& _c;
  uint32_t _period;
  uint32_t _next = 0;

  // error/backoff tracking
  uint32_t _backoffMs = 0;
  uint8_t  _consecErr = 0;
  uint8_t  _consecOk  = 0;

  RingBuffer _buf;          // <-- now a ring buffer of Records
  uint32_t   _lastFlush = 0;
  const uint32_t _flushEveryMs = 15000;  // print/drain every 15s

  void applyBackoff();
  void clearBackoff();
};
