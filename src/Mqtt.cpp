#include "Mqtt.h"
#include <ArduinoJson.h>
#include "ConfigUpdate.h"
void ensureMqtt() {
  while (!client.connected()) {
    String cid = String("esp32-") + String((uint32_t)ESP.getEfuseMac(), HEX);
    // LWT
    if (client.connect(cid.c_str(), MQTT_USER, MQTT_PASS, t_status.c_str(), 1, true, "offline")) {
      client.publish(t_status.c_str(), "online", true);

      client.subscribe(t_config.c_str(), 0);
      client.subscribe(t_ack.c_str(), 0);
    } else {
      delay(2000);
    }
  }
}

void handleCmd(char* topic, byte* payload, unsigned int len) {
  if(strcmp(topic, t_config.c_str()) == 0) {
    if(SaveConfig(payload,len)) {
        client.publish(t_ack.c_str(), "Config updated", true);
        } else {
        client.publish(t_ack.c_str(), "Config update failed", true);
        }
    return;
    
  } else if(strcmp(topic, t_ack.c_str()) == 0) {
    
    String ackMsg;
    for (unsigned int i = 0; i < len; i++) {
      ackMsg += (char)payload[i];
    }
    Serial.printf("Received ACK: %s\n", ackMsg.c_str());
  }
}