#include "Mqtt.h"
#include <ArduinoJson.h>
#include "ConfigUpdate.h"


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

void ensureMqtt() {
  while (!client.connected()) {
    String cid = String("esp32-") + String((uint32_t)ESP.getEfuseMac(), HEX);
    // LWT
    if (client.connect(cid.c_str(), MQTT_USER, MQTT_PASS, t_status.c_str(), 1, true, "offline")) {
      client.publish(t_status.c_str(), "online", true);

      client.subscribe(t_config.c_str(), 0);
      client.subscribe(t_ack.c_str(), 0);
      client.subscribe(t_write.c_str(), 0);
    } else {
      delay(2000);
    }
  }
}

void handleCmd(char* topic, byte* payload, unsigned int len) {

  uint8_t mtype = 0;
  std::vector<uint8_t> plain;
  Serial.printf("Message arrived [%s] len=%d: ", topic, len);
  Serial.println();
  
  Serial.println((char*)payload);
  if (!mqttDecrypt(payload, len, mtype, plain)) {
    Serial.println("[SEC] dropping: decrypt/verify failed");
    return;
  }
  if(strcmp(topic, t_config.c_str()) == 0) {

    ErrorCode result = SaveConfig(plain.data(), plain.size());
    String ackMsg;
    switch (result)
    {
      case ERR_OK:
        Serial.println("Config saved successfully");
        ackMsg = "Config saved successfully";
        break;

      case ERR_DESERIALIZE_FAILED:
        Serial.println("Config deserialization failed");
        ackMsg = "Config deserialization failed";
        break;

      case ERR_POLL_MS_FAILED:
        Serial.println("Polling period update failed");
        ackMsg = "Polling period update failed";
        break;

      case ERR_UPLOAD_MS_FAILED:
        Serial.println("Upload period update failed");
        ackMsg = "Upload period update failed";
        break;

      case ERR_BUFFER_CAPACITY_FAILED:
        Serial.println("Buffer capacity update failed");
        ackMsg = "Buffer capacity update failed";
        break;

      case ERR_REG_REQ_ID_1_FAILED:
        Serial.println("Reg request ID 1 update failed");
        ackMsg = "Reg request ID 1 update failed";
        break;

      default:
        break;
    }

    client.publish(t_ack.c_str(), ackMsg.c_str(), true);
    
    return;
    
  } else if(strcmp(topic, t_ack.c_str()) == 0) {
    
    String ackMsg;
    for (unsigned int i = 0; i < len; i++) {
      ackMsg += (char)payload[i];
    }
    Serial.printf("Received ACK: %s\n", ackMsg.c_str());
  }else if(strcmp(topic, t_write.c_str()) == 0) {
    // Handle write commands here
    String writeCmd;
    for (unsigned int i = 0; i < plain.size(); i++) {
      writeCmd += (char)plain[i];
    }
    Serial.printf("Received Write Command: %s\n", writeCmd.c_str());
    // validate the write command.
    //set a flag to indicate command received.
    // process in main loop.
    if(writeCmd.equals("WRITE")) 
    {
      writecommandreceived = true;
      Serial.println("Write command flag set to true.");
    }
  }
}


