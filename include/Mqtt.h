#pragma once
#include <PubSubClient.h>
#include <WiFi.h>
#include <mbedtls/base64.h>
#include "SecureLink.h"
#include "FotaManager.h"
extern SecureLink sec;    // defined once in your main .cpp (and sec.begin(...) called)
extern FotaManager fota;


extern const char* DEV_ID;
extern const String t_data;
extern const String t_config;
extern const String t_config_ack;
extern const String t_write;
extern const String t_write_ack;
extern const String t_fota_cmd;     
extern const String t_fota_status;
extern const String t_device_status;


extern const char* MQTT_USER;  // optional
extern const char* MQTT_PASS;  // optional

extern WiFiClient espClient;
extern PubSubClient client;

extern uint8_t FUNCTION_CODE;
extern String  ERROR_TYPE;
extern uint8_t EXCEPTION_CODE;
extern uint16_t DELAY_MS;

struct MqttTx {
  String topic;                  // where to publish
  std::vector<uint8_t> payload;  // what to publish (plain, will be encrypted)
  bool retain = false;
};

extern QueueHandle_t mqttTxQueue;

void ensureMqtt();

void handleCmd(char* topic, byte* payload, unsigned int len);

bool publishFotaJsonACK(const String& jsonPlain); // seals + publishes to t_fota_status

bool publishConfigJsonACK(const String& jsonPlain); // seals + publishes to t_config_ack

bool publishDeviceStatusJson(const String& jsonPlain); // seals + publishes to t_device_status

bool mqttEnqueue(const String& topic, const uint8_t* data, size_t len, bool retain=false);

bool encryptPayload(const uint8_t* plain, size_t len, std::vector<uint8_t>& outCipher);

