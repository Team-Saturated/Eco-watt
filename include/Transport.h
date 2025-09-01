#pragma once
#include <Arduino.h>
#include <vector>

struct TransportResult {
  bool ok = false;
  std::string body;      // for cloud
  std::vector<uint8_t> bytes; // for RS485/raw
  int status = 0;   // HTTP code or custom status
  std::string error;     // error text if any
};

class ITransport {
public:
  virtual ~ITransport() = default;
  virtual TransportResult exchange(const std::vector<uint8_t>& request) = 0;
};
