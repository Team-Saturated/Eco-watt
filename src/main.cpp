#include <Arduino.h>
#include "../include/Config.h"
#include "../include/InverterClient.h"
#include "../include/Poller.h"

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

static bool wifiConnect() {
  Serial.printf("WiFi connecting to %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries++ < 60) {
    delay(500);
    Serial.print(".");
    if (tries % 10 == 0) {
      // Retry connection every 5 seconds
      WiFi.disconnect();
      delay(100);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi OK: %s\n", WiFi.localIP().toString().c_str());
    return true;
  } else {
    Serial.println("WiFi failed");
    return false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  // Try to connect to WiFi
  if (!wifiConnect()) {
    Serial.println("Failed to connect to WiFi. Continuing without network...");
  }

  try {
#if SIMULATE
    g_transport = new CloudTransport(String(API_URL), String(AUTH_HEADER), REQ_TIMEOUT_MS);
#else
    g_transport = new Rs485Transport(RS485_SERIAL, RS485_BAUD, RS485_DE_RE_PIN, REQ_TIMEOUT_MS);
#endif

    if (!g_transport) {
      Serial.println("Failed to create transport");
      return;
    }

    g_client = new InverterClient(*g_transport);
    if (!g_client) {
      Serial.println("Failed to create inverter client");
      return;
    }

    g_poller = new Poller(*g_client, POLL_PERIOD_MS);
    if (!g_poller) {
      Serial.println("Failed to create poller");
      return;
    }

    Serial.println("Setup done successfully.");
  } catch (const std::exception& e) {
    Serial.printf("Setup failed with error: %s\n", e.what());
  }
}

void loop() {
  if (g_poller) {
    try {
      g_poller->loop(SLAVE_ID, START_ADDR, QTY_REGS);
    } catch (const std::exception& e) {
      Serial.printf("Error in main loop: %s\n", e.what());
    }
  } else {
    Serial.println("Poller not initialized. Retrying setup...");
    setup();
  }
  delay(5);
}
