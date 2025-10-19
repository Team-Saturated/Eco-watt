/**
 * @file Uploader.h
 * @brief Header file for the Uploader class that handles uploading data to the cloud.
 */

#pragma once
#include <Arduino.h>
#include <vector>
#include "Buffer.h"
#include "Config.h"
#include "SecureLink.h"
extern SecureLink sec;

/**
 * @class Uploader
 * @brief Drains a batch of records and sends them to the cloud.
 * 
 * The Uploader supports two modes:
 *  - RAW mode: posts each raw frame as {"frame": "<hex>"} to API_URL
 *  - DECODED mode: posts one JSON batch to API_BULK_URL (if not defined, falls back to RAW-per-item)
 */
class Uploader {
public:
  /**
   * @brief Construct a new Uploader object
   * 
   * @param apiUrl The API endpoint URL for uploading data
   * @param authHeader The authentication header string
   */
  Uploader(const String& apiUrl, const String& authHeader);

  /**
   * @brief Drains the batch and uploads it to the cloud.
   * 
   * @param batch Vector of records to upload (will be cleared after upload)
   * @return true if all items were sent successfully (or batch was empty)
   * @return false if upload failed
   */
  bool uploadBatch(std::vector<Record>& batch);

 

private:
  

  String _apiUrl;         ///< API endpoint URL
  String _auth;           ///< Authentication header
  uint32_t _uploads_ok = 0;  ///< Counter for successful uploads
  uint32_t _uploads_err = 0; ///< Counter for failed uploads
  int _last_http = 0;     ///< Last HTTP status code received
};




