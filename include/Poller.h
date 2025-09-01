#pragma once
#include <Arduino.h>
#include "InverterClient.h"

class Poller {
public:
  Poller(InverterClient& client, uint32_t periodMs): _c(client), _period(periodMs) {}
  void loop(uint8_t slave, uint16_t addr, uint16_t qty);

private:
  InverterClient& _c;
  uint32_t _period;
  uint32_t _next = 0;
};
