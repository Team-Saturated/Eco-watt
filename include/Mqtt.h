#pragma once
#include <PubSubClient.h>
#include <WiFi.h>
#include <mbedtls/base64.h>
#include "SecureLink.h"
#include "FotaManager.h"

/**
 * @file Mqtt.h
 * @brief MQTT communication interface with encryption support for IoT device
 * 
 * This header provides MQTT connectivity with secure message transmission,
 * device configuration, FOTA updates, and status reporting capabilities.
 */

/// SecureLink instance for encryption/decryption (defined in main .cpp)
extern SecureLink sec;

/// FotaManager instance for firmware updates (defined in main .cpp)
extern FotaManager fota;


extern const char* DEV_ID;
extern const String t_data;
extern const String t_config;
extern const String t_config_ack;
extern const String t_write;
extern const String t_write_ack;
extern const String t_fota_cmd;     
extern const String t_fota_status;
extern const String t_device_status;


extern const char* MQTT_USER;  // optional
extern const char* MQTT_PASS;  // optional

extern WiFiClient espClient;
extern PubSubClient client;

extern uint8_t FUNCTION_CODE;
extern String  ERROR_TYPE;
extern uint8_t EXCEPTION_CODE;
extern uint16_t DELAY_MS;

/**
 * @struct MqttTx
 * @brief Structure for MQTT transmission queue items
 * 
 * Encapsulates all data needed to publish an MQTT message,
 * including topic, payload, and retain flag.
 */
struct MqttTx {
  String topic;                  ///< MQTT topic where message will be published
  std::vector<uint8_t> payload;  ///< Message payload (plain, will be encrypted before sending)
  bool retain = false;           ///< MQTT retain flag for persistent messages
};

extern QueueHandle_t mqttTxQueue;

/**
 * @brief Ensures MQTT connection is established and maintained
 * 
 * Checks MQTT connection status and attempts reconnection if disconnected.
 * Should be called periodically in the main loop.
 */
void ensureMqtt();

/**
 * @brief Handles incoming MQTT commands
 * 
 * Callback function for processing received MQTT messages.
 * Decrypts and routes messages to appropriate handlers.
 * 
 * @param topic MQTT topic of the received message
 * @param payload Raw payload data
 * @param len Length of the payload in bytes
 */
void handleCmd(char* topic, byte* payload, unsigned int len);

/**
 * @brief Publishes FOTA acknowledgment with encrypted payload
 * 
 * Encrypts and publishes FOTA status/acknowledgment to t_fota_status topic.
 * 
 * @param jsonPlain Plain JSON string to be encrypted and published
 * @return true if publish successful, false otherwise
 */
bool publishFotaJsonACK(const String& jsonPlain);

/**
 * @brief Publishes configuration acknowledgment with encrypted payload
 * 
 * Encrypts and publishes configuration acknowledgment to t_config_ack topic.
 * 
 * @param jsonPlain Plain JSON string to be encrypted and published
 * @return true if publish successful, false otherwise
 */
bool publishConfigJsonACK(const String& jsonPlain);

/**
 * @brief Publishes device status with encrypted payload
 * 
 * Encrypts and publishes device status to t_device_status topic.
 * 
 * @param jsonPlain Plain JSON string to be encrypted and published
 * @return true if publish successful, false otherwise
 */
bool publishDeviceStatusJson(const String& jsonPlain);

/**
 * @brief Enqueues a message for MQTT transmission
 * 
 * Adds a message to the transmission queue for asynchronous publishing.
 * 
 * @param topic MQTT topic for the message
 * @param data Pointer to payload data
 * @param len Length of payload in bytes
 * @param retain MQTT retain flag (default: false)
 * @return true if successfully enqueued, false if queue is full
 */
bool mqttEnqueue(const String& topic, const uint8_t* data, size_t len, bool retain=false);

/**
 * @brief Encrypts payload data using SecureLink
 * 
 * Encrypts plain data and stores the ciphertext in the output vector.
 * 
 * @param plain Pointer to plain data to encrypt
 * @param len Length of plain data in bytes
 * @param outCipher Output vector to store encrypted data
 * @return true if encryption successful, false otherwise
 */
bool encryptPayload(const uint8_t* plain, size_t len, std::vector<uint8_t>& outCipher);

