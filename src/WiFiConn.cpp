#include "WiFiConn.h"
#include <WiFi.h>
#include "Config.h"


void wifiConnect() {
  Serial.printf("[WIFI] Connecting to SSID: %s\n", WIFI_SSID);
  
  // Enhanced WiFi debugging
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);
  
  Serial.println("[WIFI] Starting connection attempt...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint8_t tries = 0;
  uint8_t maxTries = 20;  // Reduced timeout for faster recovery
  
  while (WiFi.status() != WL_CONNECTED && tries < maxTries) {
    delay(500);
    tries++;
    
    // Show progress every 2 seconds
    if (tries % 4 == 0) {
      wl_status_t status = WiFi.status();
      Serial.printf("[WIFI] Attempt %d/%d - Status: %d ", tries, maxTries, status);
      
      // Decode WiFi status
      switch(status) {
        case WL_IDLE_STATUS:     Serial.println("(IDLE)"); break;
        case WL_NO_SSID_AVAIL:   Serial.println("(NO_SSID_AVAILABLE)"); break;
        case WL_SCAN_COMPLETED:  Serial.println("(SCAN_COMPLETED)"); break;
        case WL_CONNECTED:       Serial.println("(CONNECTED)"); break;
        case WL_CONNECT_FAILED:  Serial.println("(CONNECT_FAILED)"); break;
        case WL_CONNECTION_LOST: Serial.println("(CONNECTION_LOST)"); break;
        case WL_DISCONNECTED:    Serial.println("(DISCONNECTED)"); break;
        default:                 Serial.println("(UNKNOWN)"); break;
      }
    } else {
      Serial.print(".");
    }
    
    // More aggressive retry - every 6 attempts instead of 10
    if (tries % 6 == 0 && tries < maxTries) {
      Serial.println("\n[WIFI] Aggressive retry - resetting connection...");
      WiFi.disconnect();
      delay(200);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WIFI] Connected successfully!\n");
    Serial.printf("[WIFI] IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WIFI] Signal Strength: %d dBm\n", WiFi.RSSI());
    Serial.printf("[WIFI] MAC Address: %s\n", WiFi.macAddress().c_str());
  } else {
    Serial.printf("\n[WIFI] Connection failed after %d attempts\n", tries);
    Serial.printf("[WIFI] Final status: %d\n", WiFi.status());
    Serial.println("[WIFI] Please check:");
    Serial.println("[WIFI] 1. SSID name is correct");
    Serial.println("[WIFI] 2. Password is correct"); 
    Serial.println("[WIFI] 3. Router is in range");
    Serial.println("[WIFI] 4. Router supports 2.4GHz WiFi");
  }
}

bool checkConnection()
{
  wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    // Periodically show connection health
    static uint32_t lastHealthCheck = 0;
    if (millis() - lastHealthCheck > 60000) {  // Every 60 seconds
      Serial.printf("[WIFI] Connection healthy - RSSI: %d dBm\n", WiFi.RSSI());
      lastHealthCheck = millis();
    }
    return true;
  } else {
    Serial.printf("[WIFI] Connection lost - Status: %d ", status);
    switch(status) {
      case WL_IDLE_STATUS:     Serial.println("(IDLE)"); break;
      case WL_NO_SSID_AVAIL:   Serial.println("(NO_SSID_AVAILABLE)"); break;
      case WL_SCAN_COMPLETED:  Serial.println("(SCAN_COMPLETED)"); break;
      case WL_CONNECT_FAILED:  Serial.println("(CONNECT_FAILED)"); break;
      case WL_CONNECTION_LOST: Serial.println("(CONNECTION_LOST)"); break;
      case WL_DISCONNECTED:    Serial.println("(DISCONNECTED)"); break;
      default:                 Serial.println("(UNKNOWN)"); break;
    }
    return false;
  }
}