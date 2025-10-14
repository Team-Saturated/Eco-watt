#pragma once
//
// FotaManager (Direct-to-OTA, resumable, version-aware) — ESP32 (Arduino core)
// - Writes firmware DIRECTLY into the selected OTA partition (no filesystem).
// - Resumes exactly from the last saved offset after power loss (no erase on resume).
// - Version-aware: a different (version,size,sha) manifest resets state & starts from 0.
// - Integrity: running SHA-256 while pending; on resume, re-hash existing partition prefix.
// - Authenticity: compare to expected SHA (add your signature check at manifest time if needed).
// - Rollback-safe: sets boot partition, keeps pending=1; new image must call
//   bootSelfTestFinalize(pass) to mark valid or auto-rollback.
//
// Usage (in your sketch):
//   LittleFS/SPIFFS NOT REQUIRED for this direct-OTA version.
//   In setup(): fota.begin(); fota.bootSelfTestFinalize(/*pass=*/true or false);
//   On manifest: fota.handleManifest(ver, size, chunk, sha32, nonceOrNull);
//   On chunks:   fota.handleChunk(offset, buf, len);  // strictly sequential offsets
//   On finish:   bool ok, shaOk; uint8_t dig[32];
//                ok = fota.finishAndVerify(shaOk, dig); if (ok && shaOk) fota.requestReboot();
//
// Notes:
//   - Requires a partition table with at least two OTA slots (ota_0/ota_1) and rollback enabled.
//   - Make sure your Arduino-ESP32 core/IDF exposes esp_partition_* and esp_ota_* APIs.
//   - This implementation uses sector-aligned ERASE ONLY on fresh manifest; never on resume.
//

#include <Arduino.h>
#include <Preferences.h>
#include <mbedtls/sha256.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>

// ------------- NVS keys / config -------------
#define FOTA_NS              "fota"
#define FOTA_OFF_KEY         "off"       // uint64: next offset to write
#define FOTA_SIZE_KEY        "size"      // uint32: expected total size
#define FOTA_VER_KEY         "ver"       // string: version tag
#define FOTA_SHA_KEY         "sha"       // 32B: expected SHA-256
#define FOTA_NONCE_KEY       "nonce"     // optional 16B (if you use it)
#define FOTA_PENDING_KEY     "pend"      // u8: 1 while transfer pending
#define FOTA_PART_LABEL_KEY  "partlbl"   // char[]: label of chosen OTA partition (e.g., "ota_0")

#ifndef SPI_FLASH_SEC_SIZE
#define SPI_FLASH_SEC_SIZE   4096
#endif

template<typename T>
static inline T fota_min(T a, T b) { return a < b ? a : b; }

class FotaManager {
 public:
  bool begin() {
    _prefsReady = prefs.begin(FOTA_NS, /*readOnly=*/false);
    if (!_prefsReady) { Serial.println("[FOTA] prefs.begin failed!"); return false; }

    _next_off = prefs.getULong64(FOTA_OFF_KEY, 0);
    _total    = prefs.getULong(FOTA_SIZE_KEY,   0);
    _pending  = prefs.getUChar(FOTA_PENDING_KEY, 0);
    _version  = prefs.getString(FOTA_VER_KEY, "");

    if (_pending) {
      // Load expected SHA and partition label
      if (prefs.getBytes(FOTA_SHA_KEY, _expect, sizeof(_expect)) != 32) {
        Serial.println("[FOTA] Missing expected SHA; clearing state.");
        clearState();
        return true;
      }
      char label[32] = {0};
      prefs.getString(FOTA_PART_LABEL_KEY, label, sizeof(label));
      if (label[0] == '\0') {
        Serial.println("[FOTA] Missing OTA partition label; clearing state.");
        clearState();
        return true;
      }
      _ota_part = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
                                           ESP_PARTITION_SUBTYPE_ANY,
                                           label);
      if (!_ota_part) {
        Serial.printf("[FOTA] OTA partition '%s' not found; clearing.\n", label);
        clearState();
        return true;
      }

      // Rebuild SHA up to _next_off so we can keep hashing additional bytes
      if (!rehashPartitionPrefix(_next_off)) {
        Serial.println("[FOTA] Rehash(partition) failed; clearing state.");
        clearState();
      } else {
        _sha_active = true;
        Serial.printf("[FOTA] Resume @ %llu / %u (ver=%s, part=%s)\n",
                      (unsigned long long)_next_off, _total,
                      _version.c_str(), _ota_part->label ? _ota_part->label : "?");
      }
    }
    return true;
  }

  void clearState() {
    prefs.remove(FOTA_OFF_KEY);
    prefs.remove(FOTA_SIZE_KEY);
    prefs.remove(FOTA_VER_KEY);
    prefs.remove(FOTA_SHA_KEY);
    prefs.remove(FOTA_NONCE_KEY);
    prefs.remove(FOTA_PART_LABEL_KEY);
    prefs.putUChar(FOTA_PENDING_KEY, 0);

    _next_off   = 0;
    _total      = 0;
    _pending    = 0;
    _version    = "";
    _sha_active = false;
    _ota_part   = nullptr;
  }

  // -------- Manifest: choose partition, maybe erase, init SHA --------
  // sha256: expected 32-byte digest; nonce may be nullptr if unused
  bool handleManifest(const String& version, uint32_t size, uint32_t chunkSize,
                      const uint8_t sha256[32], const uint8_t* nonce) {
    // Decide if this is SAME job (resume) or NEW job (start from 0)
    bool same_job = (_pending &&
                     _total   == size &&
                     _version == version &&
                     memcmp(sha256, _expect, 32) == 0);

    if (!same_job) {
      // New version OR metadata changed -> start fresh
      _next_off = 0;
      _total    = size;
      _version  = version;
      memcpy(_expect, sha256, 32);
      if (nonce) memcpy(_nonce, nonce, 16);

      // Pick the next update partition (ota_0 or ota_1 alternating)
      _ota_part = esp_ota_get_next_update_partition(nullptr);
      if (!_ota_part) { Serial.println("[FOTA] No OTA partition available"); return false; }

      // ERASE once, sector-aligned to fit the image
      size_t erase_len = ((size + SPI_FLASH_SEC_SIZE - 1) / SPI_FLASH_SEC_SIZE) * SPI_FLASH_SEC_SIZE;
      esp_err_t er = esp_partition_erase_range(_ota_part, 0, erase_len);
      if (er != ESP_OK) {
        Serial.printf("[FOTA] erase failed: %d\n", er);
        return false;
      }

      // Init running SHA (pending session)
      mbedtls_sha256_init(&_sha);
      mbedtls_sha256_starts_ret(&_sha, 0);
      _sha_active = true;

      // Persist basics for resume
      prefs.putULong64(FOTA_OFF_KEY,       _next_off);
      prefs.putULong   (FOTA_SIZE_KEY,     _total);
      prefs.putString  (FOTA_VER_KEY,      _version);
      prefs.putBytes   (FOTA_SHA_KEY,      _expect, 32);
      if (nonce) prefs.putBytes(FOTA_NONCE_KEY, _nonce, 16);
      prefs.putUChar   (FOTA_PENDING_KEY,  1);
      if (_ota_part->label) prefs.putString(FOTA_PART_LABEL_KEY, _ota_part->label);
      _pending = 1;

      Serial.printf("[FOTA] Manifest (fresh). size=%u chunk=%u ver=%s part=%s\n",
                    size, chunkSize, _version.c_str(),
                    _ota_part->label ? _ota_part->label : "?");
      Serial.print  ("[FOTA] Expect SHA256: ");
      for (int i=0;i<32;i++) Serial.printf("%02X", _expect[i]);
      Serial.println();
      return true;
    }

    // SAME job -> resume (no erase)
    if (!_ota_part) {
      // recover partition from NVS if not already set
      char label[32] = {0};
      prefs.getString(FOTA_PART_LABEL_KEY, label, sizeof(label));
      _ota_part = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_ANY, label);
      if (!_ota_part) { Serial.println("[FOTA] Resume: partition not found"); return false; }
    }

    if (!_sha_active) {
      if (!rehashPartitionPrefix(_next_off)) {
        Serial.println("[FOTA] Resume: rehash failed"); return false;
      }
      _sha_active = true;
    }

    Serial.printf("[FOTA] Manifest (resume). offset=%llu/%u ver=%s part=%s\n",
                  (unsigned long long)_next_off, _total, _version.c_str(),
                  _ota_part->label ? _ota_part->label : "?");
    return true;
  }

  // -------- Chunk write (sequential) --------
  bool handleChunk(uint64_t offset, const uint8_t* data, size_t len) {
    if (!_pending) { Serial.println("[FOTA] handleChunk: not pending"); return false; }
    if (offset != _next_off) {
      Serial.printf("[FOTA] handleChunk: offset mismatch. exp=%llu got=%llu\n",
                    (unsigned long long)_next_off, (unsigned long long)offset);
      return false;
    }
    if (!_ota_part) { Serial.println("[FOTA] no OTA partition"); return false; }

    // Hash then write to flash at the given offset
    ensureShaActive();
    mbedtls_sha256_update_ret(&_sha, data, len);

    esp_err_t er = esp_partition_write(_ota_part, (size_t)offset, data, len);
    if (er != ESP_OK) {
      Serial.printf("[FOTA] partition_write failed: %d\n", er);
      return false;
    }

    _next_off += len;
    prefs.putULong64(FOTA_OFF_KEY, _next_off);
    return true;
  }

  // -------- Finish: finalize SHA, compare, select boot partition --------
  // Returns true if boot partition set successfully; shaOk indicates digest match.
  bool finishAndVerify(bool& shaOk, uint8_t outDigest[32]) {
    if (!_pending) { Serial.println("[FOTA] finishAndVerify: not pending"); return false; }
    if (_next_off != _total) {
      Serial.printf("[FOTA] finishAndVerify: incomplete %llu/%u\n",
                    (unsigned long long)_next_off, _total);
      return false;
    }
    if (!_ota_part) { Serial.println("[FOTA] finishAndVerify: no OTA partition"); return false; }

    // Finalize SHA
    ensureShaActive();
    mbedtls_sha256_finish_ret(&_sha, outDigest);
    mbedtls_sha256_free(&_sha);
    _sha_active = false;

    Serial.print("[FOTA] Computed SHA256: ");
    for (int i=0;i<32;i++) Serial.printf("%02X", outDigest[i]);
    Serial.println();

    uint8_t expect[32]; prefs.getBytes(FOTA_SHA_KEY, expect, 32);
    Serial.print("[FOTA] Expected  SHA256: ");
    for (int i=0;i<32;i++) Serial.printf("%02X", expect[i]);
    Serial.println();

    shaOk = (memcmp(outDigest, expect, 32) == 0);
    if (!shaOk) {
      Serial.println("[FOTA] ✗ SHA256 mismatch. Aborting and clearing.");
      clearState();
      return false;
    }

    // Optional: quick header sanity (esp_image_header_t) could be read & checked here.

    // Select the written partition for next boot
    esp_err_t er = esp_ota_set_boot_partition(_ota_part);
    if (er != ESP_OK) {
      Serial.printf("[FOTA] set_boot_partition failed: %d\n", er);
      clearState();
      return false;
    }

    Serial.println("[FOTA] ✓ SHA OK. Boot partition set. Awaiting reboot…");

    // Reset offset NOW as requested, but keep pending=1 until new app marks itself valid
    _next_off = 0;
    prefs.putULong64(FOTA_OFF_KEY, _next_off);
    return true;
  }

  void requestReboot() {
    Serial.println("[FOTA] Rebooting in 1500 ms…");
    delay(1500);
    esp_restart();
  }

  // Call early at boot of ANY app. If new image is pending verify, mark valid or rollback.
  void bootSelfTestFinalize(bool pass) {
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t st = ESP_OTA_IMG_UNDEFINED;
    esp_err_t r = esp_ota_get_state_partition(running, &st);

    if (r != ESP_OK) {
      if (_pending && !pass) {
        esp_ota_mark_app_invalid_rollback_and_reboot(); // never returns
      }
      return;
    }

    if (st == ESP_OTA_IMG_PENDING_VERIFY) {
      if (pass) {
        if (esp_ota_mark_app_valid_cancel_rollback() == ESP_OK) {
          // New image accepted — cleanup artifacts/state
          clearState();
        } else {
          esp_ota_mark_app_invalid_rollback_and_reboot(); // never returns
        }
      } else {
        esp_ota_mark_app_invalid_rollback_and_reboot();   // never returns
      }
    }
  }

  // ------ Accessors ------
  uint64_t nextOffset() const { return _next_off; }
  uint32_t totalSize()  const { return _total; }
  bool     pending()    const { return _pending != 0; }
  String   version()    const { return _version; }
  const char* targetPartitionLabel() const { return _ota_part && _ota_part->label ? _ota_part->label : ""; }

 private:
  // Rebuild SHA by hashing the first `upto` bytes already written to the OTA partition
  bool rehashPartitionPrefix(uint64_t upto) {
    mbedtls_sha256_init(&_sha);
    mbedtls_sha256_starts_ret(&_sha, 0);
    if (upto == 0) return true;

    if (!_ota_part) return false;

    const size_t BUF = 4096;
    std::unique_ptr<uint8_t[]> buf(new uint8_t[BUF]);
    uint64_t done = 0;
    while (done < upto) {
      size_t n = (size_t)fota_min<uint64_t>(BUF, upto - done);
      esp_err_t er = esp_partition_read(_ota_part, done, buf.get(), n);
      if (er != ESP_OK) return false;
      mbedtls_sha256_update_ret(&_sha, buf.get(), n);
      done += n;
    }
    return true;
  }

  void ensureShaActive() {
    if (!_sha_active) {
      // Rebuild from partition to current offset if needed (should be rare)
      if (!rehashPartitionPrefix(_next_off)) {
        Serial.println("[FOTA] ensureShaActive: rehash failed");
      }
      _sha_active = true;
    }
  }

 private:
  Preferences prefs;
  bool _prefsReady = false;

  // Persistent/resume data
  uint64_t _next_off = 0;
  uint32_t _total    = 0;
  uint8_t  _pending  = 0;
  String   _version;

  // SHA management (active only while pending)
  mbedtls_sha256_context _sha;
  bool _sha_active = false;
  uint8_t _expect[32]{};
  uint8_t _nonce[16]{};

  // Target OTA partition (chosen on fresh manifest; recovered by label on resume)
  const esp_partition_t* _ota_part = nullptr;
};
