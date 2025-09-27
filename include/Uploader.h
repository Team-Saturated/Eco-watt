/**
 * @file Uploader.h
 * @brief Cloud data upload manager for EcoWatt solar inverter data.
 * 
 * This file defines the Uploader class which manages batch uploads of
 * inverter data to cloud APIs. Supports multiple upload modes (raw Modbus
 * frames or decoded values) with comprehensive error tracking and retry logic.
 */

#pragma once
#include <Arduino.h>
#include <vector>
#include "Buffer.h"
#include "Config.h"

/**
 * @class Uploader
 * @brief Manages batch uploads of inverter data to cloud endpoints.
 * 
 * The Uploader class handles the transmission of buffered inverter data to
 * cloud APIs. It supports two upload modes:
 * - RAW mode: Uploads each Modbus frame as individual JSON objects with hex encoding
 * - DECODED mode: Uploads batches of decoded register values as consolidated JSON
 * 
 * Includes comprehensive metrics tracking and HTTP status monitoring for
 * debugging and system health monitoring.
 */
class Uploader {
public:
  /**
   * @brief Construct a new Uploader object.
   * 
   * @param apiUrl Base URL for the cloud API endpoint
   * @param authHeader HTTP authorization header for API authentication
   */
  Uploader(const String& apiUrl, const String& authHeader);

  /**
   * @brief Upload a batch of records to the cloud.
   * 
   * Processes and uploads all records in the provided batch according to
   * the configured upload mode (UPLOAD_MODE from Config.h). The batch
   * vector is drained/cleared after successful upload.
   * 
   * @param batch Vector of records to upload (will be cleared on success)
   * @return true if all items were sent successfully or batch was empty, false on error
   * 
   * @note Batch size is limited by MAX_BATCH_BYTES configuration parameter
   * @see Config.h for UPLOAD_MODE and MAX_BATCH_BYTES settings
   */
  bool uploadBatch(std::vector<Record>& batch);

  /**
   * @brief Get count of successful upload operations.
   * @return Number of successful uploads since initialization
   */
  uint32_t uploads_ok() const { return _uploads_ok; }

  /**
   * @brief Get count of failed upload operations.
   * @return Number of failed uploads since initialization
   */
  uint32_t uploads_err() const { return _uploads_err; }

  /**
   * @brief Get HTTP status code from last upload attempt.
   * @return Last HTTP status code (200, 404, 500, etc.) or 0 if no request made
   */
  int      last_http_status() const { return _last_http; }

private:
  /**
   * @brief Upload raw Modbus frames as individual JSON objects.
   * 
   * Sends each record's raw hex frame as a separate HTTP POST request
   * in the format: {"frame": "<hex_string>"}
   * 
   * @param batch Vector of records containing raw frame data
   * @return true if all frames uploaded successfully, false otherwise
   */
  bool uploadRawFrames(const std::vector<Record>& batch);

  /**
   * @brief Upload decoded register values as a consolidated JSON batch.
   * 
   * Combines all decoded register values into a single JSON payload
   * and uploads as one HTTP POST request to the bulk API endpoint.
   * 
   * @param batch Vector of records containing decoded register data
   * @return true if batch upload succeeded, false otherwise
   */
  bool uploadDecodedBatch(const std::vector<Record>& batch);

  String _apiUrl;           ///< Base API URL for uploads
  String _auth;             ///< HTTP authorization header
  uint32_t _uploads_ok = 0; ///< Count of successful uploads
  uint32_t _uploads_err = 0;///< Count of failed uploads
  int _last_http = 0;       ///< Last HTTP status code received
};
