#pragma once
#include <PubSubClient.h>
#include <WiFi.h>
#include <mbedtls/base64.h>
#include "SecureLink.h"
extern SecureLink sec;    // defined once in your main .cpp (and sec.begin(...) called)



extern const char* DEV_ID;
extern String t_data;
extern String t_status;
extern String t_config;
extern String t_ack;
extern String t_write;

extern const char* MQTT_USER;  // optional
extern const char* MQTT_PASS;  // optional

extern WiFiClient espClient;
extern PubSubClient client;

void ensureMqtt();
void handleCmd(char* topic, byte* payload, unsigned int len);

static bool mqttDecrypt(const uint8_t* in, size_t inLen,
                        uint8_t& outType, std::vector<uint8_t>& outPlain) {
  // Legacy plaintext guard (first byte '{' or '[')
  if (inLen && (in[0] == '{' || in[0] == '[')) {
    outPlain.assign(in, in + inLen);
    outType = 0;
    return true;
  }

  // Try base64 decode first
  size_t need = 0;
  int rc = mbedtls_base64_decode(nullptr, 0, &need, in, inLen);
  std::vector<uint8_t> sealed;
  if (rc == MBEDTLS_ERR_BASE64_INVALID_CHARACTER) {
    // Not base64 → treat as raw sealed
    sealed.assign(in, in + inLen);
  } else {
    sealed.resize(need);
    size_t outLen = 0;
    rc = mbedtls_base64_decode(sealed.data(), sealed.size(), &outLen, in, inLen);
    if (rc != 0) { Serial.printf("[SEC] b64 decode rc=%d\n", rc); return false; }
    sealed.resize(outLen);
  }

  if (!sec.open(sealed.data(), sealed.size(), outType, outPlain)) {
    Serial.println("[SEC] open() failed (HMAC/replay/decrypt)");
    return false;
  }
  return true;
}
