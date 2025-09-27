/**
 * @file Config.h
 * @brief Configuration parameters for the EcoWatt solar inverter data acquisition system.
 * 
 * This file contains all the configuration constants used throughout the EcoWatt project,
 * including WiFi credentials, polling intervals, RS485 communication settings, buffer
 * management, upload scheduling, and error handling policies.
 * 
 * @author EcoWatt Development Team
 * @date 2025
 */

#pragma once

/** @defgroup WiFi_Config WiFi Configuration
 *  @brief WiFi network connection parameters
 *  @{
 */

/** @brief WiFi network SSID (Service Set Identifier) */
#define WIFI_SSID     "LasithWifi"     // Replace with your WiFi name

/** @brief WiFi network password for WPA/WPA2 authentication */
#define WIFI_PASSWORD "12345678"       // Replace with your WiFi password

/** @} */ // end of WiFi_Config group

/** @defgroup Timing_Config Polling and Request Timing
 *  @brief Configuration for data polling intervals and timeouts
 *  @{
 */

/** @brief Polling period in milliseconds - how often we poll the inverter for data */
#define POLL_PERIOD_MS  1000           

/** @brief HTTP/RS485 request timeout in milliseconds */
#define REQ_TIMEOUT_MS  5000           

/** @} */ // end of Timing_Config group

/** @defgroup RS485_Config RS485 Communication Settings
 *  @brief RS485 serial communication configuration (ignored when SIMULATE=1)
 *  @{
 */

/** @brief Serial port used for RS485 communication (ESP32: Serial2, ESP8266: Serial) */
#define RS485_SERIAL     Serial2       

/** @brief Baud rate for RS485 communication */
#define RS485_BAUD       9600

/** @brief GPIO pin for RS485 DE/RE (Driver Enable/Receiver Enable) control */
#define RS485_DE_RE_PIN  21            

/** @brief Modbus slave ID of the target inverter */
#define SLAVE_ID         0x11

/** @brief Starting register address for Modbus read operations */
#define START_ADDR       0x0000

/** @brief Number of consecutive registers to read in each Modbus query */
#define QTY_REGS         10

/** @} */ // end of RS485_Config group

/** @defgroup Buffer_Upload_Config Buffering and Upload Configuration
 *  @brief Settings for data buffering and cloud upload scheduling
 *  @{
 */

/** @brief Upload period in milliseconds - send buffered data every 14 seconds 
 *  @note Must be less than Poller flush period (15s) to avoid data loss */
#define UPLOAD_PERIOD_MS   14000      

/** @brief Maximum number of samples to keep in RAM buffer */
#define BUFFER_CAPACITY    128         

/** @brief Maximum payload size per upload in bytes (approximate) */
#define MAX_BATCH_BYTES    8192        

/** @} */ // end of Buffer_Upload_Config group

/** @defgroup Retry_Config Upload Retry Policy
 *  @brief Configuration for handling failed upload attempts
 *  @{
 */

/** @brief Maximum number of additional retry attempts for failed uploads */
#define UPLOAD_MAX_RETRIES  2          

/** @brief Delay between retry attempts in milliseconds */
#define RETRY_DELAY_MS      2000       

/** @} */ // end of Retry_Config group

/** @defgroup Upload_Mode_Config Upload Mode Selection
 *  @brief Configuration for choosing data format sent to cloud
 *  @{
 */

/** @brief Upload mode constant for decoded register values */
#define UPLOAD_MODE_DECODED  1

/** @brief Upload mode constant for raw Modbus frames */
#define UPLOAD_MODE_RAW      2

/** @brief Current upload mode selection (default: decoded values) */
#define UPLOAD_MODE          UPLOAD_MODE_DECODED  

/** @} */ // end of Upload_Mode_Config group

/** @defgroup Error_Backoff_Config Error Handling and Backoff Policy
 *  @brief Configuration for error recovery and exponential backoff
 *  @{
 */

/** @brief Initial backoff delay after first error in milliseconds */
#define BACKOFF_MIN_MS   2000     

/** @brief Backoff increment per consecutive error in milliseconds */
#define BACKOFF_STEP_MS  2000     

/** @brief Maximum backoff delay cap in milliseconds */
#define BACKOFF_MAX_MS   30000    

/** @brief Number of consecutive successes required to reset backoff policy */
#define ERR_RESET_AFTER  1        

/** @} */ // end of Error_Backoff_Config group
