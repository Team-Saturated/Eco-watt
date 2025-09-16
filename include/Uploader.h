#pragma once
#include <Arduino.h>
#include <vector>
#include "Buffer.h"
#include "Config.h"

// Uploader drains a batch and sends it to the cloud.
// It supports two modes:
//  - RAW mode: posts each raw frame as {"frame": "<hex>"} to API_URL
//  - DECODED mode: posts one JSON batch to API_BULK_URL (if not defined, falls back to RAW-per-item)
class Uploader {
public:
  Uploader(const String& apiUrl, const String& authHeader);

  // Drains 'batch' and uploads it.
  // Returns true if all items were sent successfully (or batch was empty).
  bool uploadBatch(std::vector<Record>& batch);

  // Metrics
  uint32_t uploads_ok() const { return _uploads_ok; }
  uint32_t uploads_err() const { return _uploads_err; }
  int      last_http_status() const { return _last_http; }

private:
  bool uploadRawFrames(const std::vector<Record>& batch);
  bool uploadDecodedBatch(const std::vector<Record>& batch);

  String _apiUrl;
  String _auth;
  uint32_t _uploads_ok = 0;
  uint32_t _uploads_err = 0;
  int _last_http = 0;
};
