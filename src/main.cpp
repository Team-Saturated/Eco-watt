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
  Serial.printf("\nConnecting to WiFi: %s\n", WIFI_SSID);
  
  // Disconnect if already connected
  WiFi.disconnect(true);
  delay(1000);
  
  // Set WiFi mode
  WiFi.mode(WIFI_STA);
  
  // Configure power save mode
#if !defined(ESP8266)
  // ESP32 specific
  WiFi.setSleep(false);  // Disable power saving
#endif
  
  // Start connection
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // Wait for connection with timeout
  int tries = 0;
  const int maxTries = 30;  // 30 seconds timeout
  
  while (WiFi.status() != WL_CONNECTED && tries < maxTries) {
    delay(1000);
    Serial.print(".");
    tries++;
    
    if (tries % 5 == 0) {  // Every 5 seconds
      Serial.printf("\nAttempt %d/%d - Reconnecting...\n", tries, maxTries);
      WiFi.disconnect(true);
      delay(1000);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("WiFi Connected!\n");
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Signal Strength (RSSI): %d dBm\n", WiFi.RSSI());
    return true;
  } else {
    Serial.println("WiFi Connection Failed!");
    Serial.println("Please check your WiFi credentials and signal strength.");
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
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi connection lost! Reconnecting...");
    if (!wifiConnect()) {
      delay(5000);  // Wait 5 seconds before retrying
      return;
    }
  }

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
