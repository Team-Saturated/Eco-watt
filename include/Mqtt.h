#pragma once
#include <PubSubClient.h>
#include <WiFi.h>



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