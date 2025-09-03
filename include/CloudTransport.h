#pragma once
#include "Transport.h"

class CloudTransport : public ITransport {
public:
  CloudTransport(const String& url, const String& auth, uint32_t timeoutMs);
  TransportResult exchange(const std::vector<uint8_t>& request) override;

private:
  String _url;
  String _auth;
  uint32_t _timeout;
};
