#include <Arduino.h>
#include "Config.h"
#include "InverterClient.h"
#include "Poller.h"

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#else
  #include <WiFi.h>
#endif

#include "CloudTransport.h"
#include "Rs485Transport.h"

#if SIMULATE
CloudTransport* g_transport = nullptr;
#else
Rs485Transport* g_transport = nullptr;
#endif

InverterClient* g_client = nullptr;
Poller* g_poller = nullptr;

static void wifiConnect() {
  Serial.printf("WiFi connecting to %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries++ < 60) {
    delay(500); Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi OK: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("WiFi failed");
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  wifiConnect();

#if SIMULATE
  g_transport = new CloudTransport(String(API_URL), String(AUTH_HEADER), REQ_TIMEOUT_MS);
#else
  g_transport = new Rs485Transport(RS485_SERIAL, RS485_BAUD, RS485_DE_RE_PIN, REQ_TIMEOUT_MS);
#endif

  g_client = new InverterClient(*g_transport);
  g_poller = new Poller(*g_client, POLL_PERIOD_MS);

  Serial.println("Setup done.");
}

void loop() {
  g_poller->loop(SLAVE_ID, START_ADDR, QTY_REGS);
  delay(5);
}
