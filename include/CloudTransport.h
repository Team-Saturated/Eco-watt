#pragma once
#include "Transport.h"

class CloudTransport : public ITransport {
public:
  CloudTransport(const String& read_url, const String& write_url, const String& auth, uint32_t timeoutMs);
  TransportResult exchange(const std::vector<uint8_t>& request, const bool isWrite) override;

private:
  String _read_url;
  String _write_url;
  String _auth;
  uint32_t _timeout;
};

extern bool writeemulationreceived;
extern uint8_t FUNCTION_CODE;
extern String  ERROR_TYPE;
extern uint8_t EXCEPTION_CODE;
extern uint16_t DELAY_MS;