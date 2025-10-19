#pragma once
#include <WiFi.h>

/**
 * @file WiFiConn.h
 * @brief WiFi connection management for ESP32
 * 
 * This file contains functions for establishing and monitoring
 * WiFi connections on ESP32 devices.
 */

/**
 * @brief Establishes a WiFi connection
 * 
 * Attempts to connect to a WiFi network using predefined credentials.
 * This function will block until a connection is established or fails.
 */
void wifiConnect();

/**
 * @brief Checks the current WiFi connection status
 * 
 * Verifies whether the device is currently connected to a WiFi network.
 * 
 * @return true if connected to WiFi, false otherwise
 */
bool checkConnection();