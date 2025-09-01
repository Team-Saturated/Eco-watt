#pragma once
#include "Transport.h"
#include <string>

class CloudTransport : public ITransport {
public:
  // Core ctor (std::string)
  CloudTransport(const std::string& url, const std::string& auth, uint32_t timeoutMs);
  // Convenience ctor (Arduino String) so main.cpp can pass String(...)
  CloudTransport(const String& url, const String& auth, uint32_t timeoutMs);

  TransportResult exchange(const std::vector<uint8_t>& request) override;

private:
  std::string _url, _auth;
  uint32_t _timeout;
};
