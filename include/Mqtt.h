#pragma once
#include <PubSubClient.h>
#include <WiFi.h>
#include <mbedtls/base64.h>
#include "SecureLink.h"
#include "FotaManager.h"
extern SecureLink sec;    // defined once in your main .cpp (and sec.begin(...) called)
extern FotaManager fota;


extern const char* DEV_ID;
extern String t_data;
extern String t_status;
extern String t_config;
extern String t_ack;
extern String t_write;
extern String t_fota_cmd;     
extern String t_fota_status;  
extern String t_fota_log;     


extern const char* MQTT_USER;  // optional
extern const char* MQTT_PASS;  // optional

extern WiFiClient espClient;
extern PubSubClient client;

struct MqttTx {
  String topic;                  // where to publish
  std::vector<uint8_t> payload;  // what to publish (plain, will be encrypted)
  bool retain = false;
};

extern QueueHandle_t mqttTxQueue;

void ensureMqtt();
void handleCmd(char* topic, byte* payload, unsigned int len);
// FOTA helpers

bool publishFotaJson(const String& jsonPlain); // seals + publishes to t_fota_status

bool mqttEnqueue(const String& topic, const uint8_t* data, size_t len, bool retain=false);

bool encryptPayload(const uint8_t* plain, size_t len, std::vector<uint8_t>& outCipher);