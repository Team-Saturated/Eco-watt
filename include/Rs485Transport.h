#pragma once
#include "Transport.h"

class Rs485Transport : public ITransport {
public:
  Rs485Transport(HardwareSerial& ser, uint32_t baud, int deRePin, uint32_t timeoutMs);
  TransportResult exchange(const std::vector<uint8_t>& request) override;

private:
  HardwareSerial& _ser;
  uint32_t _timeout;
  int _deRe;
  void setTx(bool en);
};
