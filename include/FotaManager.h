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
    prefs.begin(FOTA_NS, false);
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
    mbedtls_sha256_init(&_sha); mbedtls_sha256_starts(&_sha, 0);
    _next_off = 0; _total = size;

    Serial.printf("[FOTA] Manifest: size=%u, version=%s\n", size, version.c_str());
  
    

    // Persist manifest essentials (resume-safe)
    prefs.putULong64(FOTA_OFF_KEY, _next_off);
    prefs.putULong(FOTA_SIZE_KEY, _total);
    prefs.putString(FOTA_VER_KEY, version);
    prefs.putBytes(FOTA_SHA_KEY, sha256, 32);
    prefs.putBytes(FOTA_NONCE_KEY, nonce, 16);
    prefs.putUChar(FOTA_PENDING_KEY, 1);
    _pending = 1;

    return true;
  }

  // --- Chunk ---
  // data: raw firmware bytes (already HMAC-checked at app-layer)
  bool handleChunk(uint64_t offset, const uint8_t* data, size_t len) {
    if (!_pending) return false;
    if (offset != _next_off) return false; // out-of-order

    size_t wrote = Update.write(const_cast<uint8_t*>(data), len);
    if (wrote != len) return false;
    
    mbedtls_sha256_update(&_sha, data, len);

    _next_off += len;
    prefs.putULong64(FOTA_OFF_KEY, _next_off);
    return true;
  }

  // --- Finish & verify SHA256 ---
  bool finishAndVerify(bool& shaOk, uint8_t outDigest[32]) {
    if (!_pending) return false;
    if (_next_off != _total) return false;
    mbedtls_sha256_finish(&_sha, outDigest);

    uint8_t expect[32]; prefs.getBytes(FOTA_SHA_KEY, expect, 32);
    shaOk = (memcmp(outDigest, expect, 32) == 0);

    if (shaOk) {
      if (!Update.end()) return false;
      // Ready to reboot into new partition; mark pending app
      return true;
    } else {
      Update.abort();
      clearState();
      return false;
    }
  }

  // Controlled reboot request (cloud tells you) — optional gate
  void requestReboot() {
    // Just set a flag if you want to delay reboot; or reboot now:
    esp_restart();
  }

  // Call from setup() on every boot to finalize or rollback
 

void bootSelfTestFinalize(bool pass) {
    // Ask bootloader what state this app is in
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    esp_err_t r = esp_ota_get_state_partition(running, &state);
    if (r != ESP_OK) {
    // If we can't read state, be conservative: roll back if we *think* we were pending
    if (_pending && !pass) {
    esp_ota_mark_app_invalid_rollback_and_reboot();
    }
    return;
    }
    // Only finalize/rollback when the bootloader says this image is PENDING_VERIFY
    if (state == ESP_OTA_IMG_PENDING_VERIFY) {
    if (pass) {
    // Commit this image
    esp_err_t ok = esp_ota_mark_app_valid_cancel_rollback();
    if (ok == ESP_OK) {
        clearState();  // clear our NVS breadcrumbs (offset/sha/version/nonce/pending)
        // (optional) report "boot_ok" to cloud here
    } else {
        // Could not mark valid; safest is to roll back
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
    } else {
    // Self-test failed → revert
    esp_ota_mark_app_invalid_rollback_and_reboot();
    }
    return;
}

    // Not in PENDING_VERIFY:
    // - Either we’re running an already-valid app
    // - Or we rebooted without finishing an OTA
    // Keep local state tidy:
    if (!_pending) return;

    // If we still had _pending=1 but bootloader doesn’t think we’re pending,
    // it means we hadn’t rebooted into the new image yet (download phase) or state drifted.
    // Do NOT clear offset/sha/version here; they’re needed to resume a download.
    // Only clear after a successful verify+commit (handled above).
    }

  uint64_t nextOffset() const { return _next_off; }
  uint32_t totalSize() const  { return _total; }
  bool pending() const        { return _pending != 0; }

 private:
  Preferences prefs;
  uint64_t _next_off = 0;
  uint32_t _total    = 0;
  uint8_t  _pending  = 0;
  mbedtls_sha256_context _sha;
};
