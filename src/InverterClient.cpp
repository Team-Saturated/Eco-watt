#include "InverterClient.h"
#include "Modbus.h"

TransportResult InverterClient::readHolding(uint8_t slave, uint16_t addr, uint16_t qty) {
  auto frame = Modbus::buildRead03(slave, addr, qty);
  return _t.exchange(frame);
}
