/**
 * @file WiFi_conn.h
 * @brief WiFi connection management utilities for ESP32/ESP8266 platforms.
 * 
 * This file provides cross-platform WiFi connection functions that work
 * with both ESP32 and ESP8266 microcontrollers. Handles initial connection
 * establishment and connection monitoring for the EcoWatt system.
 */

#pragma once
#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#else
  #include <WiFi.h>
#endif

/**
 * @brief Establish initial WiFi connection using configured credentials.
 * 
 * Attempts to connect to the WiFi network using the SSID and password
 * defined in Config.h. This function handles the initial connection
 * establishment and provides status feedback.
 * 
 * @return true if connection was established successfully, false otherwise
 * 
 * @note Uses WIFI_SSID and WIFI_PASSWORD constants from Config.h
 * @see Config.h for WiFi credential configuration
 */
bool  wifiConnect();

/**
 * @brief Ensure WiFi connection is active, reconnect if necessary.
 * 
 * Monitors the current WiFi connection status and automatically attempts
 * to reconnect if the connection has been lost. This function should be
 * called periodically in the main loop to maintain network connectivity.
 * 
 * @note Non-blocking function that handles reconnection attempts gracefully
 * @see wifiConnect() for initial connection establishment
 */
void ensureWiFiConnected();