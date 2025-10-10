#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <Update.h>
#include <mbedtls/sha256.h>
#include <esp_ota_ops.h>

// NVS keys
#define FOTA_NS            "fota"
#define FOTA_OFF_KEY       "off"
#define FOTA_SIZE_KEY      "size"
#define FOTA_VER_KEY       "ver"
#define FOTA_SHA_KEY       "sha"   // 32B
#define FOTA_NONCE_KEY     "nonce" // 16B
#define FOTA_PENDING_KEY   "pend"  // uint8 0/1

class FotaManager {
 public:
  bool begin() {
    _prefsReady = prefs.begin(FOTA_NS, /*readOnly=*/false);
    if (!_prefsReady) {
      Serial.println("[FOTA] prefs.begin failed!");
      return false;
    }
    _next_off = prefs.getULong64(FOTA_OFF_KEY, 0);
    _total    = prefs.getULong(FOTA_SIZE_KEY, 0);
    _pending  = prefs.getUChar(FOTA_PENDING_KEY, 0);
    return true;
  }

  void clearState() {
    prefs.remove(FOTA_OFF_KEY);
    prefs.remove(FOTA_SIZE_KEY);
    prefs.remove(FOTA_VER_KEY);
    prefs.remove(FOTA_SHA_KEY);
    prefs.remove(FOTA_NONCE_KEY);
    prefs.putUChar(FOTA_PENDING_KEY, 0);
    _next_off=0; _total=0; _pending=0;
  }
// --- Manifest ---
bool handleManifest(const String& version, uint32_t size, uint32_t chunkSize,
                    const uint8_t sha256[32], const uint8_t nonce[16]) {
  if (!Update.begin(size)) {
    Serial.printf("[FOTA] Update.begin() FAILED. Error: %s\n", Update.errorString());
    Serial.printf("[FOTA] Requested size: %u bytes\n", size);
    return false;
  }

  // Reset running SHA-256 (use *_ret variants)
  mbedtls_sha256_init(&_sha);
  mbedtls_sha256_starts_ret(&_sha, 0);   // 0 = SHA-256

  _next_off = 0;
  _total    = size;
  _pending  = 1;

  Serial.printf("[FOTA] Manifest: size=%u, version=%s, chunk=%u\n",
                size, version.c_str(), chunkSize);

  Serial.print("[FOTA] Expected SHA256 (incoming): ");
  for (int i=0;i<32;i++) Serial.printf("%02X", sha256[i]);
  Serial.println();

  // Avoid stale leftovers (belt & braces)
  prefs.remove(FOTA_SHA_KEY);
  prefs.remove(FOTA_NONCE_KEY);

  // Persist manifest essentials (resume-safe)
  prefs.putULong64(FOTA_OFF_KEY, _next_off);
  prefs.putULong   (FOTA_SIZE_KEY, _total);
  prefs.putString  (FOTA_VER_KEY,  version);
  prefs.putBytes   (FOTA_SHA_KEY,  sha256, 32);   // <-- raw 32 bytes
  prefs.putBytes   (FOTA_NONCE_KEY,nonce,  16);
  prefs.putUChar   (FOTA_PENDING_KEY, 1);

  // Read-back probe (must be 32)
  uint8_t probe[32];
  size_t got = prefs.getBytes(FOTA_SHA_KEY, probe, sizeof(probe));
  Serial.printf("[FOTA] NVS SHA len=%u : ", (unsigned)got);
  for (size_t i=0;i<got;i++) Serial.printf("%02X", probe[i]);
  Serial.println();
  if (got != 32) {
    Serial.println("[FOTA] ERROR: NVS stored SHA not 32 bytes");
    return false;
  }

  return true;
}
// --- Chunk ---
bool handleChunk(uint64_t offset, const uint8_t* data, size_t len) {
  if (!_pending) { Serial.println("[FOTA] handleChunk: not pending"); return false; }
  if (offset != _next_off) {
    Serial.printf("[FOTA] handleChunk: offset mismatch. Expected %llu, got %llu\n",
                  (unsigned long long)_next_off, (unsigned long long)offset);
    return false;
  }

  // Feed EXACT bytes to running SHA first (or after, but same bytes)
  mbedtls_sha256_update_ret(&_sha, data, len);

  size_t wrote = Update.write(const_cast<uint8_t*>(data), len);
  if (wrote != len) {
    Serial.printf("[FOTA] handleChunk: write failed. Expected %zu, wrote %zu\n", len, wrote);
    return false;
  }

  _next_off += len;
  prefs.putULong64(FOTA_OFF_KEY, _next_off);
  return true;
}

  // --- Finish & verify SHA256 ---
  bool finishAndVerify(bool& shaOk, uint8_t outDigest[32]) {
    if (!_pending){
      Serial.println("[FOTA] finishAndVerify: not pending");
      return false;
    }
    
    if (_next_off != _total) {
      Serial.printf("[FOTA] finishAndVerify: incomplete transfer. Received %llu/%u bytes\n", 
                    _next_off, _total);
      return false;
    }
    
    // Finalize SHA256
    mbedtls_sha256_finish(&_sha, outDigest);
    mbedtls_sha256_free(&_sha);
    
    // Print computed SHA256
    Serial.print("[FOTA] Computed SHA256:  ");
    for(int i=0; i<32; i++) Serial.printf("%02X", outDigest[i]);
    Serial.println();
    
    // Get expected SHA256
    uint8_t expect[32]; 
    prefs.getBytes(FOTA_SHA_KEY, expect, 32);
    
    Serial.print("[FOTA] Expected SHA256:  ");
    for(int i=0; i<32; i++) Serial.printf("%02X", expect[i]);
    Serial.println();
    
    // Compare
    shaOk = (memcmp(outDigest, expect, 32) == 0);
    
    if (shaOk) {
      Serial.println("[FOTA] ✓ SHA256 MATCH!");
      
      if (!Update.end()) {
        Serial.printf("[FOTA] Update.end() FAILED. Error: %s\n", Update.errorString());
        return false;
      }
      
      Serial.println("[FOTA] Update.end() SUCCESS");
      return true;
      
    } else {
      Serial.println("[FOTA] ✗ SHA256 MISMATCH!");
      
      // Show byte-by-byte differences
      Serial.println("[FOTA] Differences:");
      for(int i=0; i<32; i++) {
        if (outDigest[i] != expect[i]) {
          Serial.printf("  Byte %d: got %02X, expected %02X\n", i, outDigest[i], expect[i]);
        }
      }
      
      Update.abort();
      clearState();
      return false;
    }
  }

  void requestReboot() {
    Serial.println("[FOTA] Rebooting in 2 seconds...");
    delay(2000);
    esp_restart();
  }

  void bootSelfTestFinalize(bool pass) {
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    esp_err_t r = esp_ota_get_state_partition(running, &state);
    
    if (r != ESP_OK) {
      if (_pending && !pass) {
        esp_ota_mark_app_invalid_rollback_and_reboot();
      }
      return;
    }
    
    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
      if (pass) {
        esp_err_t ok = esp_ota_mark_app_valid_cancel_rollback();
        if (ok == ESP_OK) {
          clearState();
        } else {
          esp_ota_mark_app_invalid_rollback_and_reboot();
        }
      } else {
        esp_ota_mark_app_invalid_rollback_and_reboot();
      }
      return;
    }
  }

  uint64_t nextOffset() const { return _next_off; }
  uint32_t totalSize() const  { return _total; }
  bool pending() const        { return _pending != 0; }

 private:
  Preferences prefs;
  bool _prefsReady = false;
  uint64_t _next_off = 0;
  uint32_t _total    = 0;
  uint8_t  _pending  = 0;
  mbedtls_sha256_context _sha;
};