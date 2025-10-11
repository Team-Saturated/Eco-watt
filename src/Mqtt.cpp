#include "Mqtt.h"
#include <ArduinoJson.h>
#include "ConfigUpdate.h"
#include "Config.h"

static String bytesToHex(const uint8_t *p, size_t n)
{
  String s;
  s.reserve(n * 2);
  for (size_t i = 0; i < n; i++)
  {
    char b[3];
    snprintf(b, sizeof(b), "%02X", p[i]);
    s += b;
  }
  return s;
}

static String toBase64(const uint8_t *data, size_t len)
{
  size_t need = 0;
  (void)mbedtls_base64_encode(nullptr, 0, &need, data, len);
  std::unique_ptr<uint8_t[]> out(new uint8_t[need + 1]);
  size_t outLen = 0;
  if (mbedtls_base64_encode(out.get(), need, &outLen, data, len) != 0)
    return String();
  out[outLen] = 0;
  return String((char *)out.get());
}

bool publishFotaJsonACK(const String &jsonPlain)
{
  // Seal with SecureLink then Base64 (same style as your Uploader) :contentReference[oaicite:6]{index=6}
  std::vector<uint8_t> sealed;
  if (!sec.seal(/*type*/ 2, (const uint8_t *)jsonPlain.c_str(), jsonPlain.length(), sealed))
  {
    Serial.println("[FOTA] seal failed");
    return false;
  }
  String b64 = toBase64(sealed.data(), sealed.size());
  // if (b64.length() > 0) client.setBufferSize((uint16_t)(b64.length() + 64));
  return client.publish(t_fota_status.c_str(), b64.c_str(), false);
}

bool publishConfigJsonACK(const String &jsonPlain)
{
  // Seal with SecureLink then Base64 (same style as your Uploader)
  std::vector<uint8_t> sealed;
  if (!sec.seal(/*type*/ 2, (const uint8_t *)jsonPlain.c_str(), jsonPlain.length(), sealed))
  {
    Serial.println("[CONFIG] seal failed");
    return false;
  }
  String b64 = toBase64(sealed.data(), sealed.size());
  // if (b64.length() > 0) client.setBufferSize((uint16_t)(b64.length() + 64));
  return client.publish(t_config_ack.c_str(), b64.c_str(), false);
}


static inline String u64dec(uint64_t v)
{
  char b[21]; // up to 20 digits + NUL
  snprintf(b, sizeof(b), "%llu", (unsigned long long)v);
  return String(b);
}
static inline String u64hex(uint64_t v)
{
  char b[17];
  snprintf(b, sizeof(b), "%016llX", (unsigned long long)v);
  return String(b);
}

static bool mqttDecrypt(const uint8_t *in, size_t inLen,
                        uint8_t &outType, std::vector<uint8_t> &outPlain)
{
  // Legacy plaintext guard (first byte '{' or '[')
  if (inLen && (in[0] == '{' || in[0] == '['))
  {
    outPlain.assign(in, in + inLen);
    outType = 0;
    return true;
  }

  // Try base64 decode first
  size_t need = 0;
  int rc = mbedtls_base64_decode(nullptr, 0, &need, in, inLen);
  std::vector<uint8_t> sealed;
  if (rc == MBEDTLS_ERR_BASE64_INVALID_CHARACTER)
  {
    // Not base64 → treat as raw sealed
    sealed.assign(in, in + inLen);
  }
  else
  {
    sealed.resize(need);
    size_t outLen = 0;
    rc = mbedtls_base64_decode(sealed.data(), sealed.size(), &outLen, in, inLen);
    if (rc != 0)
    {
      Serial.printf("[SEC] b64 decode rc=%d\n", rc);
      return false;
    }
    sealed.resize(outLen);
  }

  if (!sec.open(sealed.data(), sealed.size(), outType, outPlain))
  {
    Serial.println("[SEC] open() failed (HMAC/replay/decrypt)");
    return false;
  }
  return true;
}

// Minimal JSON escape for Arduino String
static String jsonEscape(const String& s) {
  String out; out.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s[i];
    switch (c) {
      case '\"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\b': out += "\\b";  break;
      case '\f': out += "\\f";  break;
      case '\n': out += "\\n";  break;
      case '\r': out += "\\r";  break;
      case '\t': out += "\\t";  break;
      default:
        if ((unsigned char)c < 0x20) {  // control chars -> \u00XX
          char buf[7];
          snprintf(buf, sizeof(buf), "\\u%04X", (unsigned char)c);
          out += buf;
        } else {
          out += c;
        }
    }
  }
  return out;
}




void ensureMqtt()
{
  while (!client.connected())
  {
    String cid = String("esp32-") + String((uint32_t)ESP.getEfuseMac(), HEX);
    // LWT
    if (client.connect(cid.c_str(), MQTT_USER, MQTT_PASS, t_config_ack.c_str(), 1, true, "offline"))
    {
      client.publish(t_config_ack.c_str(), "online", true);

      client.subscribe(t_config.c_str(), 0);
      //client.subscribe(t_ack.c_str(), 0);
      client.subscribe(t_write.c_str(), 0);
      client.subscribe(t_fota_cmd.c_str(), 1);
    }
    else
    {
      delay(2000);
    }
  }
}

void configReceived(std::vector<uint8_t> &plain)
{
  ErrorCode result = SaveConfig(plain.data(), plain.size());
  String ackMsg;
  switch (result)
  {
  case ERR_OK:
    Serial.println("Config saved successfully");
    ackMsg = "ERR_OK";
    break;

  case ERR_DESERIALIZE_FAILED:
    Serial.println("Config deserialization failed");
    ackMsg = "ERR_DESERIALIZE_FAILED";
    break;

  case ERR_POLL_MS_FAILED:
    Serial.println("Polling period update failed");
    ackMsg = "ERR_POLL_MS_FAILED";
    break;

  case ERR_UPLOAD_MS_FAILED:
    Serial.println("Upload period update failed");
    ackMsg = "ERR_UPLOAD_MS_FAILED";
    break;

  case ERR_BUFFER_CAPACITY_FAILED:
    Serial.println("Buffer capacity update failed");
    ackMsg = "ERR_BUFFER_CAPACITY_FAILED";
    break;

  case ERR_REG_REQ_ID_1_FAILED:
    Serial.println("Reg request ID 1 update failed");
    ackMsg = "ERR_REG_REQ_ID_1_FAILED";
    break;

  default:
    break;
  }
  
  publishConfigJsonACK("{\"ack\":\"" + ackMsg + "\"}");


  return;
}

void ackReceived(std::vector<uint8_t> &plain)
{
  String ackMsg;
  for (unsigned int i = 0; i < plain.size(); i++)
  {
    ackMsg += (char)plain[i];
  }
  Serial.printf("Received ACK: %s\n", ackMsg.c_str());
}

void writeReceived(std::vector<uint8_t> &plain)
{

  Serial.printf("Received Write Command: %s\n", plain.data());
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, plain.data(), plain.size());
  Serial.println("Deserialize Json: " + String(error.c_str()));
  if (error)
  {
    Serial.println("Failed to parse JSON");
    return;
  }
  if (doc.containsKey("op") && doc.containsKey("address") &&
      doc.containsKey("value"))
  {
    const char* op = doc["op"];
    WRITE_ADDR = doc["address"];
    WRITE_VALUE = doc["value"];
    if (strcmp(op, "write") == 0)
    {
      writecommandreceived = true;
      Serial.println("Write command flag set to true.");
    }
  }
}

void fotaReceived(std::vector<uint8_t> &plain)
{
  StaticJsonDocument<8192> doc;
  DeserializationError err = deserializeJson(doc, (const char *)plain.data(), plain.size());
  if (err)
  {
    Serial.printf("[FOTA] bad json: %s\n", err.c_str());
    publishFotaJsonACK("{\"ev\":\"error\",\"reason\":\"bad_json\"}");
    return;
  }

  const char *op = doc["op"] | "";
  Serial.printf("[FOTAAAAA] cmd op='%s'\n", op);
  if (!strcmp(op, "manifest"))
  {
    const char *version = doc["version"] | "unknown";
    uint32_t size = doc["size"] | 0;
    uint32_t csize = doc["chunk"] | 4096;
    // sha256_hex -> 32 bytes
    const char *sha_hex = doc["sha256_hex"] | "";
    uint8_t sha[32] = {0};
    if (strlen(sha_hex) == 64)
    {
      for (int i = 0; i < 32; i++)
      {
        unsigned v;
        sscanf(sha_hex + 2 * i, "%02x", &v);
        sha[i] = (uint8_t)v;
      }
    }
    else
    {
      publishFotaJsonACK("{\"ev\":\"error\",\"reason\":\"sha_len\"}");
      return;
    }

    // nonce_b64 -> 16 bytes
    uint8_t nonce[16] = {0};
    size_t nb = 0;
    const char *nonce_b64 = doc["nonce_b64"] | "";
    if (mbedtls_base64_decode(nonce, sizeof(nonce), &nb, (const unsigned char *)nonce_b64, strlen(nonce_b64)) != 0 || nb != 16)
    {
      publishFotaJsonACK("{\"ev\":\"error\",\"reason\":\"nonce_len\"}");
      return;
    }

    if (!fota.handleManifest(String(version), size, csize, sha, nonce))
    {
      publishFotaJsonACK("{\"ev\":\"error\",\"reason\":\"manifest_refused\"}");
      return;
    }
    // Tell cloud where to start sending
    String st = String("{\"ev\":\"need_chunks\",\"next_offset\":") + u64dec((uint64_t)fota.nextOffset()) +
                ",\"total\":" + String(fota.totalSize()) + ",\"version\":\"" + version + "\"}";
    Serial.println("This is st: " + st);
    publishFotaJsonACK(st);
    return;
  }

  if (!strcmp(op, "chunk"))
  {
    uint64_t offset = doc["offset"] | 0ULL;

    const char *b64 = doc["data_b64"] | "";
    
    size_t b64_len = strlen(b64);
    if (b64_len == 0)
    {
      publishFotaJsonACK("{\"ev\":\"error\",\"reason\":\"chunk_empty\"}");
      return;
    }
    size_t need = 0;
    int rc = mbedtls_base64_decode(nullptr, 0, &need, (const unsigned char *)b64, b64_len);

    // mbedtls_base64_decode returns MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL when called with nullptr
    // This is expected behavior - it sets 'need' to the required size
    if (rc != 0 && rc != MBEDTLS_ERR_BASE64_BUFFER_TOO_SMALL)
    {
      Serial.printf("[FOTA] base64 size calculation failed, rc=%d\n", rc);
      publishFotaJsonACK("{\"ev\":\"error\",\"reason\":\"chunk_b64_invalid\"}");
      return;
    }
    std::vector<uint8_t> buf(need);
    size_t outLen = 0;
    if (mbedtls_base64_decode(buf.data(), buf.size(), &outLen, (const unsigned char *)b64, strlen(b64)) != 0)
    {
      publishFotaJsonACK("{\"ev\":\"error\",\"reason\":\"chunk_b64_decode\"}");
      return;
    }
    buf.resize(outLen);

    if (!fota.handleChunk(offset, buf.data(), buf.size()))
    {
      String st = String("{\"ev\":\"error\",\"reason\":\"chunk_apply\",\"expect\":") +
                  u64dec((uint64_t)fota.nextOffset()) + "}";
      publishFotaJsonACK(st);
      return;
    }

    // Progress
    float pct = (fota.totalSize() == 0) ? 0.f : (100.0f * (float)fota.nextOffset() / (float)fota.totalSize());
    String st = String("{\"ev\":\"progress\",\"next_offset\":") + u64dec((uint64_t)fota.nextOffset()) +
                ",\"total\":" + String(fota.totalSize()) + ",\"pct\":" + String(pct, 2) + "}";
    publishFotaJsonACK(st);
    return;
  }

  if (!strcmp(op, "finish"))
  {
    bool shaOk = false;
    uint8_t digest[32];
    if (!fota.finishAndVerify(shaOk, digest))
    {
      publishFotaJsonACK("{\"ev\":\"verify_fail\",\"reason\":\"finish_failed\"}");
      return;
    }
    if (!shaOk)
    {
      publishFotaJsonACK("{\"ev\":\"verify_fail\",\"reason\":\"sha_mismatch\"}");
      return;
    }
    String dhex = String("{\"ev\":\"verify_ok\",\"sha256_hex\":\"") + bytesToHex(digest, 32) + "\"}";
    publishFotaJsonACK(dhex);
    // Wait for explicit "reboot" op (controlled reboot). You can auto-reboot if desired.
    return;
  }

  if (!strcmp(op, "reboot"))
  {
    publishFotaJsonACK("{\"ev\":\"rebooting\"}");
    // Controlled reboot → ESP32 will boot new partition in PENDING_VERIFY state. :contentReference[oaicite:10]{index=10}
    fota.requestReboot(); // esp_restart()
    return;
  }

  // Unknown op
  publishFotaJsonACK("{\"ev\":\"error\",\"reason\":\"bad_op\"}");
  return;

}



void handleCmd(char *topic, byte *payload, unsigned int len)
{

  uint8_t mtype = 0;
  std::vector<uint8_t> plain;
  Serial.printf("Message arrived [%s] len=%d: ", topic, len);
  Serial.println();
  if (!mqttDecrypt(payload, len, mtype, plain))
  {
    Serial.println("[SEC] dropping: decrypt/verify failed");
    return;
  }
  if (strcmp(topic, t_config.c_str()) == 0)
  {

    configReceived(plain);
    return;
  }
  
  else if (strcmp(topic, t_write.c_str()) == 0)
  {
    writeReceived(plain);
    return;
  }
  else if (strcmp(topic, t_fota_cmd.c_str()) == 0)
  {
    fotaReceived(plain);
    return;
  }
  else
  {
    Serial.println("Unknown topic");
    return;
  }
}

bool encryptPayload(const uint8_t* plain, size_t len, std::vector<uint8_t>& outCipher) {
  // --- Example placeholder: identity (no-op). Replace with your AES/SecureLink ---
  outCipher.resize(len);
  if (len) memcpy(outCipher.data(), plain, len);
  return true;
  // If you also need Base64 after encryption:
  //  - produce binary cipher first,
  //  - then Base64 encode into a new vector<uint8_t> (or publish as text).
}

static MqttTx* makeMsg(const String& topic, const uint8_t* data, size_t len, bool retain) {
  auto* m = new (std::nothrow) MqttTx();
  if (!m) return nullptr;
  m->topic = topic;
  m->retain = retain;
  m->payload.resize(len);
  if (len) memcpy(m->payload.data(), data, len);
  return m;
}

bool mqttEnqueue(const String& topic, const uint8_t* data, size_t len, bool retain) {
  if (!mqttTxQueue) return false;
  MqttTx* p = makeMsg(topic, data, len, retain);
  if (!p) return false;
  if (xQueueSend(mqttTxQueue, &p, 0) != pdPASS) { delete p; return false; }
  return true;
}
// ===== Publisher task (decrypts? no) → encrypts → publishes =====

