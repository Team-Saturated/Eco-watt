#pragma once
#include <Arduino.h>
#include <string>
#include <vector>

struct TransportResult {
  bool ok = false;
  String body;          // for cloud
  std::vector<uint8_t> bytes; // for RS485/raw
  int status = 0;      // HTTP code or custom status
  String error;        // error text if any
};

class ITransport {
public:
  virtual ~ITransport() = default;
  virtual TransportResult exchange(const std::vector<uint8_t>& request) = 0;
};
