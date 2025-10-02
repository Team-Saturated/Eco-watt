#include "WiFiConn.h"
#include <WiFi.h>
#include "Config.h"


void wifiConnect() {
  Serial.printf("WiFi connecting to %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint8_t tries = 0;
  while (WiFi.status() != WL_CONNECTED ) {
    delay(500);
    Serial.print(".");
    if (tries % 10 == 0) {
      WiFi.disconnect();
      delay(100);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
    tries++;
  }

  Serial.println();
}

bool checkConnection()
{
  if (WiFi.status() == WL_CONNECTED) {
    //Serial.printf("WiFi OK: %s\n", WiFi.localIP().toString().c_str());
    return true;
  } else {
    Serial.println("WiFi failed");
    return false;
  }
}