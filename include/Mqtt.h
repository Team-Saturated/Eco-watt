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

void ensureMqtt();
void handleCmd(char* topic, byte* payload, unsigned int len);
// FOTA helpers
void publishFotaStatus(const String& jsonPlainSealedBase64);
bool publishFotaJson(const String& jsonPlain); // seals + publishes to t_fota_status