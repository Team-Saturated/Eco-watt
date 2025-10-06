#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <mbedtls/md.h>
#include <mbedtls/aes.h>
#include <esp_system.h>   // esp_fill_random
#include <vector>

// ---- Tuning ----
#define SECURELINK_NAMESPACE  "sec"
#define SECURELINK_KEY_PSK    "psk"        // 32-byte PSK
#define SECURELINK_KEY_BOOT   "boot"
#define SECURELINK_KEY_SEQ    "seq"

struct __attribute__((packed)) SecureHdr {
  uint8_t   ver;          // 1
  uint8_t   type;         // app message type
  uint16_t  reserved;     // align
  uint64_t  boot_epoch;   // anti-replay
  uint64_t  seq;          // anti-replay
  uint8_t   iv[12];       // AES-CTR IV (unique per message)
};

class SecureLink {
public:
  bool begin(const String& deviceId) {
    _deviceId = deviceId;
    prefs.begin(SECURELINK_NAMESPACE, false);

    // Load/create boot_epoch
    _boot = prefs.getULong64(SECURELINK_KEY_BOOT, 0ULL) + 1ULL;
    prefs.putULong64(SECURELINK_KEY_BOOT, _boot);

    // Load seq
    _seq  = prefs.getULong64(SECURELINK_KEY_SEQ, 0ULL);

    // Load PSK (provision this once via your own method)
    size_t len = prefs.getBytesLength(SECURELINK_KEY_PSK);
    if (len != 32) { _ok=false; return false; }
    uint8_t psk[32];
    prefs.getBytes(SECURELINK_KEY_PSK, psk, 32);

    deriveKeys(psk, 32);  // -> _Kenc[16], _Kmac[32]
    _ok = true;
    return true;
  }

  // One-time provisioning helper
  bool provisionPSK(const uint8_t psk32[32]) {
    prefs.begin(SECURELINK_NAMESPACE, false);
    prefs.putBytes(SECURELINK_KEY_PSK, psk32, 32);
    return true;
  }

  // Encrypt+MAC. Returns binary blob: [SecureHdr][ciphertext][HMAC32]
  bool seal(uint8_t type, const uint8_t* plain, size_t plen, std::vector<uint8_t>& out) {
    if (!_ok) return false;
    SecureHdr hdr{};
    hdr.ver = 1;
    hdr.type = type;
    hdr.reserved = 0;
    hdr.boot_epoch = _boot;
    hdr.seq = ++_seq;
    esp_fill_random(hdr.iv, sizeof(hdr.iv));

    // AES-CTR encrypt
    std::vector<uint8_t> ct(plen);
    if (!aesCtrCrypt(hdr.iv, plain, ct.data(), plen)) return false;

    // HMAC over header||ciphertext
    uint8_t mac[32];
    if (!hmacHeaderAndData((uint8_t*)&hdr, sizeof(hdr), ct.data(), ct.size(), mac)) return false;

    // assemble output
    out.resize(sizeof(hdr) + ct.size() + sizeof(mac));
    memcpy(out.data(), &hdr, sizeof(hdr));
    memcpy(out.data()+sizeof(hdr), ct.data(), ct.size());
    memcpy(out.data()+sizeof(hdr)+ct.size(), mac, sizeof(mac));

    // persist seq occasionally
    static uint32_t flushCtr=0;
    if ((++flushCtr & 0x0F) == 0) prefs.putULong64(SECURELINK_KEY_SEQ, _seq);
    return true;
  }

  // Verify+decrypt. Input is binary blob from MQTT.
  // Returns false if HMAC fails, replay detected, or decrypt error.
  bool open(const uint8_t* buf, size_t blen, uint8_t& outType, std::vector<uint8_t>& plain) {
    if (!_ok) return false;
    if (blen < sizeof(SecureHdr)+32) return false;

    const SecureHdr* hdr = (const SecureHdr*)buf;
    const uint8_t* ct    = buf + sizeof(SecureHdr);
    const size_t  ctlen  = blen - sizeof(SecureHdr) - 32;
    const uint8_t* mac   = buf + blen - 32;

    // HMAC verify
    uint8_t tag[32];
    if (!hmacHeaderAndData((uint8_t*)hdr, sizeof(SecureHdr), (uint8_t*)ct, ctlen, tag)) return false;
    if (!constTimeEq(tag, mac, 32)) return false;

    // Anti-replay
    if (!fresh(hdr->boot_epoch, hdr->seq)) return false;

    // Decrypt
    plain.resize(ctlen);
    if (!aesCtrCrypt(hdr->iv, ct, plain.data(), ctlen)) return false;

    // accept and update replay window
    _lastBoot = hdr->boot_epoch;
    _lastSeq  = hdr->seq;
    outType   = hdr->type;
    return true;
  }

  uint64_t boot_epoch() const { return _boot; }
  uint64_t seq() const { return _seq; }

private:
  Preferences prefs;
  String _deviceId;
  bool   _ok=false;

  uint8_t _Kenc[16]; // AES-128
  uint8_t _Kmac[32]; // HMAC-SHA256
  uint64_t _boot=0, _seq=0;
  // runtime anti-replay (session)
  uint64_t _lastBoot=0, _lastSeq=0;

  // ---- helpers ----
  void deriveKeys(const uint8_t* psk, size_t pskLen) {
    // salt = SHA256(deviceId)
    uint8_t salt[32];
    sha256((const uint8_t*)_deviceId.c_str(), _deviceId.length(), salt);

    // HKDF-Extract
    uint8_t prk[32];
    hmac_sha256(salt, 32, psk, pskLen, prk);

    // HKDF-Expand to 48 bytes
    uint8_t okm[48];
    uint8_t T[32]; size_t Tlen=0; uint8_t ctr=1;
    // info = "EcoWatt"
    const uint8_t info[] = {'E','c','o','W','a','t','t'};
    // round 1
    uint8_t buf1[sizeof(T)+sizeof(info)+1];
    size_t n1 = 0;
    memcpy(buf1+n1, T, Tlen); n1+=Tlen;
    memcpy(buf1+n1, info, sizeof(info)); n1+=sizeof(info);
    buf1[n1++] = ctr++;
    hmac_sha256(prk, 32, buf1, n1, T); Tlen=32;
    memcpy(okm, T, 32);
    // round 2
    uint8_t buf2[sizeof(T)+sizeof(info)+1];
    size_t n2 = 0;
    memcpy(buf2+n2, T, Tlen); n2+=Tlen;
    memcpy(buf2+n2, info, sizeof(info)); n2+=sizeof(info);
    buf2[n2++] = ctr++;
    hmac_sha256(prk, 32, buf2, n2, T);
    memcpy(okm+32, T, 16); // need only 48 total

    memcpy(_Kenc, okm, 16);
    memcpy(_Kmac, okm+16, 32);
    secureZero(salt, sizeof(salt));
    secureZero(prk, sizeof(prk));
    secureZero(okm, sizeof(okm));
    secureZero(T, sizeof(T));
  }

  bool aesCtrCrypt(const uint8_t iv[12], const uint8_t* in, uint8_t* out, size_t len) {
    mbedtls_aes_context aes; mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_enc(&aes, _Kenc, 128) != 0) { mbedtls_aes_free(&aes); return false; }
    size_t nc_off=0; unsigned char stream_block[16]={0};
    // mbedTLS wants 16-byte nonce_counter; we pass 12B IV + 4B counter (zeroed)
    uint8_t nonce_counter[16]={0};
    memcpy(nonce_counter, iv, 12);
    int rc = mbedtls_aes_crypt_ctr(&aes, len, &nc_off, nonce_counter, stream_block, in, out);
    mbedtls_aes_free(&aes);
    return rc==0;
  }

  bool hmacHeaderAndData(const uint8_t* hdr, size_t hlen, const uint8_t* data, size_t dlen, uint8_t out[32]) {
    return hmac_sha256(_Kmac, 32, hdr, hlen, data, dlen, out);
  }

  bool fresh(uint64_t boot, uint64_t seq) {
    // accept newer boot, or same boot with strictly increasing seq
    if (boot > _lastBoot) return true;
    if (boot == _lastBoot && seq > _lastSeq) return true;
    return false;
  }

  static bool constTimeEq(const uint8_t* a, const uint8_t* b, size_t n){
    uint8_t v=0; for (size_t i=0;i<n;i++) v |= (a[i]^b[i]); return v==0;
  }

  static void secureZero(void* p, size_t n){ volatile uint8_t* v=(volatile uint8_t*)p; while(n--) *v++=0; }

  // ---- tiny crypto glue ----
  static void sha256(const uint8_t* msg, size_t mlen, uint8_t out[32]){
    mbedtls_md_context_t c; mbedtls_md_init(&c);
    mbedtls_md_setup(&c, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    mbedtls_md_starts(&c); mbedtls_md_update(&c, msg, mlen); mbedtls_md_finish(&c, out);
    mbedtls_md_free(&c);
  }
  // HMAC(key, data1||data2)
  static bool hmac_sha256(const uint8_t* key, size_t klen,
                          const uint8_t* d1, size_t d1len,
                          const uint8_t* d2, size_t d2len,
                          uint8_t out[32]) {
    mbedtls_md_context_t c; mbedtls_md_init(&c);
    if (mbedtls_md_setup(&c, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1)!=0) { mbedtls_md_free(&c); return false; }
    mbedtls_md_hmac_starts(&c, key, klen);
    if (d1 && d1len) mbedtls_md_hmac_update(&c, d1, d1len);
    if (d2 && d2len) mbedtls_md_hmac_update(&c, d2, d2len);
    mbedtls_md_hmac_finish(&c, out);
    mbedtls_md_free(&c);
    return true;
  }
  static bool hmac_sha256(const uint8_t* key, size_t klen,
                          const uint8_t* data, size_t dlen,
                          uint8_t out[32]) {
    return hmac_sha256(key,klen,data,dlen,nullptr,0,out);
  }
};
