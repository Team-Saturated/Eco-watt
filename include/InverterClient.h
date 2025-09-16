#pragma once
#include "Transport.h"
#include <vector>

class InverterClient {
public:
  explicit InverterClient(ITransport& t): _t(t) {}
  // returns raw response (cloud: body string in TransportResult, rs485: bytes)
  TransportResult readHolding(uint8_t slave, uint16_t addr, uint16_t qty);

private:
  ITransport& _t;
};
