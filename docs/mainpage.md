/**
 * @mainpage EcoWatt Solar Inverter Data Acquisition System
 * 
 * @section intro_sec Introduction
 * 
 * EcoWatt is a comprehensive data acquisition system designed for monitoring
 * solar inverters using ESP32/ESP8266 microcontrollers. The system provides
 * real-time data collection, buffering, and cloud upload capabilities with
 * robust error handling and recovery mechanisms.
 * 
 * @section features_sec Key Features
 * 
 * - **Multi-Protocol Support**: RS485 Modbus RTU and HTTP-based cloud APIs
 * - **Robust Data Buffering**: Ring buffer with configurable capacity and overflow handling
 * - **Cloud Integration**: Automatic upload of inverter data to cloud endpoints
 * - **Error Recovery**: Exponential backoff and retry logic for failed operations
 * - **Cross-Platform**: Compatible with ESP32 and ESP8266 microcontrollers
 * - **Flexible Configuration**: Comprehensive configuration system via Config.h
 * - **Real-time Monitoring**: Continuous polling with configurable intervals
 * 
 * @section architecture_sec System Architecture
 * 
 * The EcoWatt system is built with a modular architecture consisting of several
 * key components:
 * 
 * ### Core Components
 * 
 * - **Transport Layer** (@ref ITransport): Abstract interface for communication protocols
 * - **Inverter Client** (@ref InverterClient): High-level inverter communication interface
 * - **Data Acquisition** (@ref Acquisition): Modbus data acquisition and decoding
 * - **Buffer Management** (@ref RingBuffer): Circular buffer for data storage
 * - **Polling Controller** (@ref Poller): Periodic data acquisition with error handling
 * - **Cloud Uploader** (@ref Uploader): Batch upload manager for cloud APIs
 * 
 * ### Data Flow
 * 
 * ```
 * Solar Inverter → [RS485/HTTP] → InverterClient → Acquisition → RingBuffer → Uploader → Cloud API
 * ```
 * 
 * @section configuration_sec Configuration
 * 
 * The system is configured through constants defined in Config.h:
 * 
 * - **WiFi Settings**: Network credentials and connection parameters
 * - **Polling Configuration**: Data acquisition timing and timeouts
 * - **RS485 Parameters**: Serial communication settings for Modbus
 * - **Buffer Management**: Memory allocation and overflow policies
 * - **Upload Scheduling**: Cloud synchronization intervals and batch sizes
 * - **Error Handling**: Retry policies and backoff strategies
 * 
 * @section usage_sec Quick Start
 * 
 * ### Hardware Setup
 * 
 * 1. Connect ESP32/ESP8266 to solar inverter via RS485 interface
 * 2. Configure GPIO pins for RS485 DE/RE control
 * 3. Ensure stable power supply and WiFi connectivity
 * 
 * ### Software Configuration
 * 
 * 1. Update WiFi credentials in Config.h:
 *    ```cpp
 *    #define WIFI_SSID     "YourNetworkName"
 *    #define WIFI_PASSWORD "YourPassword"
 *    ```
 * 
 * 2. Configure RS485 parameters:
 *    ```cpp
 *    #define RS485_DE_RE_PIN  21    // Your DE/RE control pin
 *    #define SLAVE_ID         0x11  // Inverter Modbus address
 *    ```
 * 
 * 3. Set cloud API endpoints and authentication
 * 
 * ### Basic Usage
 * 
 * ```cpp
 * #include "Config.h"
 * #include "Poller.h"
 * #include "Uploader.h"
 * 
 * void setup() {
 *   // Initialize WiFi connection
 *   wifiConnect();
 *   
 *   // Create components
 *   RingBuffer buffer(BUFFER_CAPACITY);
 *   Rs485Transport transport;
 *   InverterClient client(transport);
 *   Poller poller(client, POLL_PERIOD_MS, buffer);
 *   
 *   // Start data acquisition
 * }
 * 
 * void loop() {
 *   // Continuous polling and upload
 *   poller.loop(SLAVE_ID, START_ADDR, QTY_REGS);
 *   delay(100);
 * }
 * ```
 * 
 * @section modules_sec Module Documentation
 * 
 * ### Communication Modules
 * - @ref Transport.h "Transport Layer" - Protocol abstraction interface
 * - @ref InverterClient.h "Inverter Client" - High-level communication API
 * - @ref Rs485Transport.h "RS485 Transport" - Modbus RTU implementation
 * - @ref CloudTransport.h "Cloud Transport" - HTTP API implementation
 * 
 * ### Data Processing Modules
 * - @ref Acquisition.h "Data Acquisition" - Modbus frame processing and decoding
 * - @ref Buffer.h "Buffer Management" - Circular buffer for data storage
 * - @ref Modbus.h "Modbus Utilities" - Low-level Modbus protocol functions
 * 
 * ### System Control Modules
 * - @ref Poller.h "Polling Controller" - Periodic data acquisition manager
 * - @ref Uploader.h "Cloud Uploader" - Batch upload and synchronization
 * - @ref WiFi_conn.h "WiFi Management" - Network connection utilities
 * 
 * ### Configuration and Utilities
 * - @ref Config.h "System Configuration" - Global configuration constants
 * - @ref ErrorCodes.h "Error Handling" - Modbus exception codes and messages
 * 
 * @section performance_sec Performance Considerations
 * 
 * ### Memory Management
 * - Ring buffer with configurable capacity (default: 128 records)
 * - Automatic overflow handling with drop-oldest policy
 * - Flash string storage for error messages to conserve RAM
 * 
 * ### Network Efficiency
 * - Batch uploads to minimize HTTP overhead
 * - Configurable upload intervals and payload sizes
 * - Automatic reconnection and retry logic
 * 
 * ### Error Recovery
 * - Exponential backoff for failed operations
 * - Comprehensive error classification for targeted handling
 * - Graceful degradation under adverse conditions
 * 
 * @section troubleshooting_sec Troubleshooting
 * 
 * ### Common Issues
 * 
 * **Communication Failures**:
 * - Check RS485 wiring and termination resistors
 * - Verify Modbus slave address and register ranges
 * - Monitor baud rate and parity settings
 * 
 * **WiFi Connectivity**:
 * - Verify network credentials and signal strength
 * - Check firewall settings for outbound HTTPS
 * - Monitor reconnection attempts in serial output
 * 
 * **Cloud Upload Issues**:
 * - Validate API endpoints and authentication tokens
 * - Check payload format and size limits
 * - Review HTTP status codes in debug output
 * 
 * ### Debug Tools
 * - Serial console output for real-time monitoring
 * - Built-in metrics for upload success/failure rates
 * - Comprehensive error classification and reporting
 * 
 * @section license_sec License
 * 
 * This project is distributed under the terms specified in the LICENSE file.
 * Please refer to the project repository for complete licensing information.
 * 
 * @section contact_sec Support and Contribution
 * 
 * For support, bug reports, or contributions, please visit the project
 * repository or contact the EcoWatt development team.
 * 
 * @author EcoWatt Development Team
 * @date 2025
 * @version 1.0
 */