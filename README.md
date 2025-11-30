# EcoWatt IoT Solar Monitoring System

A comprehensive IoT platform for real-time solar inverter monitoring and management using ESP32 microcontrollers with advanced data compression, encryption, and cloud connectivity.

## Project Overview

EcoWatt is a production-ready IoT system that monitors solar inverters through Modbus RTU communication, compresses telemetry data with 40% compression, encrypts all communications with AES-256, and provides real-time visualization through a web dashboard. The system features automatic light sleep power optimization for extended battery life in field deployments,  firmware over-the-air updates, dynamic configuration management, and remote register control.

## Project Structure

```
Eco-watt/
├── 📁 .git/                           # Git version control
├── 📁 .pio/                           # PlatformIO build artifacts
├── 📁 .venv/                          # Python virtual environment
├── 📁 .vscode/                        # VS Code configuration
├── 📁 __pycache__/                    # Python compiled bytecode
├── 📁 docs/                           # Documentation and API reference
│   ├── 📁 docs/                       # Generated documentation
│   ├── 📁 html/                       # HTML documentation output
│   ├── 📁 latex/                      # LaTeX documentation output
│   ├── 📄 Documentation-Manager.ps1   # PowerShell doc generation script
│   ├── 📄 Doxyfile                   # Doxygen configuration
│   ├── 📄 generate_docs.bat          # Windows documentation generator
│   ├── 📄 generate_docs.sh           # Linux/macOS documentation generator
│   ├── 📄 mainpage.md                # Documentation main page
│   └── 📄 README.md                  # Documentation guide
├── 📁 include/                        # C++ header files
│   ├── 📄 Acquisition.h              # Data acquisition interface
│   ├── 📄 Buffer.h                   # Ring buffer management
│   ├── 📄 CloudTransport.h           # Cloud communication layer
│   ├── 📄 Compression.h              # Data compression algorithms
│   ├── 📄 Config.h                   # Device configuration management
│   ├── 📄 ConfigUpdate.h             # Dynamic configuration updates
│   ├── 📄 ErrorCodes.h               # System error definitions
│   ├── 📄 FotaManager.h              # Firmware Over-The-Air updates
│   ├── 📄 InverterClient.h           # Solar inverter communication
│   ├── 📄 InverterSim.h              # Inverter simulation for testing
│   ├── 📄 Modbus.h                   # Modbus RTU protocol implementation
│   ├── 📄 Mqtt.h                     # MQTT client interface
│   ├── 📄 Packetizer.h               # Data packetization utilities
│   ├── 📄 Poller.h                   # Periodic data polling
│   ├── 📄 Rs485Transport.h           # RS485 hardware interface
│   ├── 📄 SecureLink.h               # Cryptographic functions
│   ├── 📄 Transport.h                # Communication transport layer
│   ├── 📄 Uploader.h                 # Data upload management
│   ├── 📄 WiFiConn.h                 # WiFi connectivity management
│   └── 📄 README                     # Include directory documentation
├── 📁 lib/                           # External libraries
│   └── 📄 README                     # Library documentation
├── 📁 pc_shim/                       # PC simulation shims
│   ├── 📄 Arduino.h                  # Arduino API simulation
│   ├── 📄 HTTPClient.h               # HTTP client simulation
│   └── 📄 WiFi.h                     # WiFi API simulation
├── 📁 results/                       # Test and benchmark results
├── 📁 src/                           # C++ source files
│   ├── 📄 Buffer.cpp                 # Ring buffer implementation
│   ├── 📄 CloudTransport.cpp         # Cloud communication implementation
│   ├── 📄 Compression.cpp            # Data compression algorithms
│   ├── 📄 ConfigUpdate.cpp           # Configuration update handler
│   ├── 📄 InverterClient.cpp         # Inverter communication logic
│   ├── 📄 main.cpp                   # ESP32 main application entry
│   ├── 📄 Modbus.cpp                 # Modbus protocol implementation
│   ├── 📄 Mqtt.cpp                   # MQTT client implementation
│   ├── 📄 Packetizer.cpp             # Data packetization logic
│   ├── 📄 Poller.cpp                 # Data polling implementation
│   ├── 📄 Rs485Transport.cpp         # RS485 hardware driver
│   ├── 📄 Uploader.cpp               # Data upload logic
│   ├── 📄 WiFiConn.cpp               # WiFi connection management
│   └── 📄 README.md                  # Source code documentation
├── 📁 test/                          # Testing and validation
│   ├── 📄 benchmark_test.py          # Performance benchmark tests
│   ├── 📄 test_upload.py             # Upload functionality tests
│   └── 📄 README.md                  # Testing documentation
├── 📁 uploads/                       # FOTA firmware storage
│   ├── 📄 upload_ESP32_BENCHMARK_*.bin  # Benchmark firmware builds
│   ├── 📄 upload_ESP32_DIRECT_*.bin     # Production firmware builds
│   └── 📄 upload_ESP32_SOLAR_INV_*.bin  # Solar inverter firmware builds
├── 📁 venv/                          # Alternative Python environment
├── 📄 .gitignore                     # Git ignore patterns
├── 📄 API_Documentation.pdf          # Complete API reference manual
├── 📄 EcoWatt - Compression_Benchmark_Report.pdf  # Performance analysis
├── 📄 LICENSE                        # Project license
├── 📄 Links.txt                      # Useful project links
├── 📄 node-red-dulmin.json          # Node-RED flow configuration
├── 📄 nodered-flow.json             # Alternative Node-RED flow
├── 📄 petrinet_flow.pdf             # System flow documentation
├── 📄 platformio.ini                # PlatformIO project configuration
├── 📄 README.md                     # This comprehensive project guide
├── 📄 server.py                     # Python Flask cloud server
├── 📄 Video_Links.md                # Project demonstration videos
```

## System Architecture

### Hardware Components
- **ESP32 Microcontroller**: WiFi-enabled with dual-core processing
- **RS485 Interface**: Industrial-grade Modbus RTU communication
- **Solar Inverter**: Modbus-compatible photovoltaic inverter
- **MQTT Broker**: Cloud messaging infrastructure
- **Development Tools**: PlatformIO IDE with comprehensive debugging

### Software Stack
- **Device Firmware**: C++ with PlatformIO framework and Arduino libraries
- **Cloud Server**: Python Flask with MQTT integration and web dashboard
- **Security Layer**: AES-CTR encryption with HMAC-SHA256 authentication
- **Data Compression**: Custom delta compression achieving 40% compression 
- **Web Interface**: Professional dashboard with real-time charts and device management
- **Documentation**: Doxygen-generated API reference with comprehensive guides
- **Testing Framework**: Python-based validation and benchmark tools

## Core Modules and Features

###  Data Acquisition System (`Acquisition.h`, `Poller.h`, `InverterClient.h`)
- **Real-time Polling**: Configurable intervals for 10 solar registers
- **Modbus RTU Integration**: Industrial-standard communication protocol
- **Fault Tolerance**: Automatic retry mechanisms and error recovery
- **Timestamp Synchronization**: Precise data correlation and validation
- **Register Management**: Bitwise selection for optimized data collection

###  Data Processing Pipeline (`Buffer.h`, `Compression.h`, `Packetizer.h`)
- **Ring Buffer Architecture**: Memory-efficient circular data storage
- **Delta Compression**: Advanced algorithm achieving 40% data reduction
- **Packet Assembly**: Structured data formatting for transmission
- **Memory Management**: Automatic rotation with configurable capacity
- **Data Integrity**: Checksum validation and corruption detection

###  Security Framework (`SecureLink.h`, `Config.h`)
- **AES-256-CTR Encryption**: Military-grade data confidentiality
- **HMAC-SHA256 Authentication**: Message integrity verification
- **Key Derivation**: HKDF with device-specific identifiers
- **Anti-Replay Protection**: Boot timestamps and sequence validation
- **Secure Configuration**: Encrypted parameter updates and storage

###  Communication Infrastructure (`WiFiConn.h`, `Mqtt.h`, `CloudTransport.h`)
- **WiFi Management**: Automatic reconnection and network optimization
- **MQTT Protocol**: Lightweight messaging for IoT connectivity
- **Cloud Integration**: Secure bidirectional communication channels
- **Transport Layer**: Abstracted communication interface
- **Network Resilience**: Connection monitoring and recovery

###  Firmware Management (`FotaManager.h`, `ConfigUpdate.h`)
- **Over-The-Air Updates**: Remote firmware deployment with progress tracking
- **Version Control**: Semantic versioning and rollback capabilities
- **Configuration Sync**: Dynamic parameter updates without restart
- **Validation Framework**: Pre-deployment integrity checks
- **Recovery Mechanisms**: Failsafe boot and emergency recovery

### Power Optimization (Real Hardware)
- **Auto Light Sleep**: ESP-IDF Power Management with FreeRTOS Tickless IDLE
- **Estimated Power Reduction**: 60-80% average consumption decrease
- **WiFi Preservation**: Connection maintained through sleep cycles
- **Dual-Mode Support**: Simulation (fast response) and Production (power optimized)

### Power Optimization: Documentations

Power Optimization Complete Explanation - Auto Light Sleep Mode  (For the Real Hardware): https://drive.google.com/file/d/1C2zzyY9uqoM2SWg5507X6_RIy-A4Gy0z/view?usp=sharing

Power Optimization Quick Guide – Auto Sleep Mode ( HW Implementation): https://drive.google.com/file/d/1R_ltQUcMEJhlEXrQH1qiKok_8bxrV7oR/view?usp=sharing


#### How Power Optimization Works

**Automatic Sleep Flow:**
```
Upload Cycle (15 minutes)
    ↓
FreeRTOS Scheduler
    ↓
All tasks blocked? → YES
    ↓
Tickless IDLE activates
    ↓
Power Management Component
    ↓
Auto Light Sleep (630s)
    ↓
RTC Timer Wakeup
    ↓
Resume Operations
```

**Sleep Window Strategy:**
```
|<--- 135s --->|<------- 630s ------->|<--- 135s --->|
| Start Buffer |   Sleep Window       | End Buffer   |
|   (15%)      |      (70%)           |   (15%)      |
|              |                      |              |
| WiFi Stable  | CPU OFF, WiFi Sleep  | Data Upload  |
0s           135s                   765s          900s
```

**Power Optimization Method:**

| Component | Implementation | Benefit |
|-----------|---------------|---------|
| **Sleep Trigger** | FreeRTOS Tickless IDLE | Automatic, no manual calls |
| **Sleep Type** | ESP32 Light Sleep | CPU halted, RAM retained |
| **WiFi Mode** | Modem Sleep (WIFI_PS_MIN_MODEM) | Connection maintained |
| **Timing** | Centered 70-15-15 split | Balance power & stability |
| **Clock Source** | External 32kHz crystal | Accurate sleep timing |
| **Configuration** | ESP-IDF PM API | One-time setup, system managed |

**Mode Comparison:**

| Aspect | Simulation Mode | Real Hardware Mode |
|--------|----------------|-------------------|
| Upload Interval | 20 seconds | 15 minutes |
| Light Sleep |  Disabled |  Enabled |
| Expected Current | ~150 mA | ~30-50 mA |
| Use Case | Testing/Demos | Production/Field |

📌 **📌 Important Note:** This power optimization design targets the separate PlatformIO environment for real hardware, not the simulation environment. Tickless IDLE and Auto Light Sleep are system-level FreeRTOS/ESP-IDF features that may not be fully supported under the Arduino core / simulated builds. We are currently working on ESP-IDF–included, environment-based power-management code integrated with the existing hardware code, with this mode serving as a backup in case any issues arise during full power-optimization  (**Reference :** [`src/main_pm_backup.cpp`](src/main_pm_backup.cpp)). Light sleep is disabled in simulation because the simulator uses high-frequency polling. Documentation contains predicted power savings; these must be validated on real hardware, especially where RS485/Modbus polling interacts with the 15-minute upload interval.



###  Web Dashboard and API (`server.py`)
- **Real-time Visualization**: Interactive charts for all 10 solar parameters
- **Device Management**: Comprehensive configuration and control interface
- **Historical Analytics**: Time-series data analysis and trending
- **FOTA Management**: Firmware upload, deployment, and monitoring
- **API Endpoints**: RESTful interface for programmatic access
- **Responsive Design**: Professional UI with mobile compatibility

## Technical Specifications

###  Performance Metrics
| Metric | Value | Description |
|--------|-------|-------------|
| **Compression Ratio** | 0.4 | Data reduction up to 40% |
| **Polling Frequency** | 1.5s intervals | Configurable inverter data collection |
| **Data Retention** | 1000 records | Ring buffer with automatic rotation |
| **Security Level** | AES-256 | Military-grade encryption standard |
| **Authentication** | HMAC-SHA256 | 256-bit message integrity |
| **Network Latency** | <100ms | Cloud communication response time |

###  Communication Protocols
- **Modbus RTU (RS485)**: Industrial standard for inverter communication
- **MQTT v3.1.1**: Lightweight messaging for cloud connectivity  
- **HTTP/HTTPS**: RESTful API and web interface
- **WebSocket**: Real-time dashboard updates
- **JSON**: Structured data exchange format
- **WiFi 802.11 b/g/n**: 2.4GHz wireless connectivity

###  Firmware Architecture
```
ESP32 Application Layer
├──  Application Core (main.cpp)
├──  Task Scheduler (FreeRTOS)
├──  Data Processing Pipeline
│   ├── Acquisition → Buffer → Compression → Upload
├──  Security Layer
│   ├── Encryption/Decryption
│   ├── Key Management
│   └── Authentication
├──  Communication Stack
│   ├── WiFi Management
│   ├── MQTT Client
│   └── HTTP Client
└──  Hardware Abstraction Layer
    ├── Modbus RTU Driver
    ├── Flash Storage
    └── GPIO Control
```

###  Cloud Server Architecture
```
Python Flask Application
├──  Web Interface (HTML/CSS/JavaScript)
├──  API Endpoints (RESTful)
├──  MQTT Integration
├──  Security Middleware
├──  Data Processing
└──  Memory Storage (with rotation)
```

## Development Workflow

###  Prerequisites and Environment Setup

#### Required Tools
```bash
# Install PlatformIO Core
pip install platformio

# Python development environment
python -m venv .venv
# Windows
.venv\Scripts\activate
# Linux/macOS  
source .venv/bin/activate

# Install Python dependencies
pip install flask paho-mqtt cryptography

# Documentation generation
# Install Doxygen from https://doxygen.nl
```

#### Development Environment
- **IDE**: VS Code with PlatformIO extension
- **Python**: 3.8+ for server development
- **ESP32**: Arduino framework with PlatformIO
- **Documentation**: Doxygen for API reference

#### Build Environments
- **`esp32dev_sim`**: Simulation mode (20s cycles, no power optimization)
#### ESP32 Firmware Development
```bash
# Navigate to project root
cd Eco-watt/

# Build firmware (simulation mode - default)
pio run -e esp32dev_sim

# Build firmware (real hardware with power optimization)
pio run -e esp32dev_hw

# Upload to device
pio run -e esp32dev_sim --target upload

# Monitor serial output
pio device monitor --port COM3 --baud 115200

# Clean build artifacts
pio run --target clean
```

**Power Optimization Notes:**
- `esp32dev_sim`: Fast testing, no sleep (20s upload cycles)
- `esp32dev_hw`: Production deployment with auto light sleep (15min cycles)
- Requires external 32.768kHz crystal on GPIO32/33 for accurate sleep timing


#### Cloud Server Development
pio run --target clean
```

#### Cloud Server Development
```bash
# Activate virtual environment
.venv\Scripts\activate  # Windows
source .venv/bin/activate  # Linux/macOS

# Install dependencies
pip install -r requirements.txt

# Run development server
python server.py

# Server will start on http://localhost:8080
```

###  Configuration Management

#### ESP32 Device Configuration
Edit `include/Config.h` for device-specific settings:
```cpp
// WiFi Credentials
#define WIFI_SSID "YourNetworkName"
#define WIFI_PASSWORD "YourPassword"

// Device Identity
#define DEVICE_ID "ESP32_SOLAR_INV_001"
#define PSK "YourPreSharedKey"

// MQTT Broker
#define MQTT_BROKER "broker.hivemq.com"
#define MQTT_PORT 1883
```

#### Server Configuration
Configure `server.py` for cloud deployment:
```python
# MQTT Settings
MQTT_BROKER = "broker.hivemq.com"
MQTT_PORT = 1883

# Device Authentication
DEVICE_PSK = "YourPreSharedKey"
DEVICE_ID = "ESP32_SOLAR_INV_001"

# Web Server
HOST = "0.0.0.0"
PORT = 8080
```

###  Deployment Process

#### Development Deployment
1. **Firmware Compilation**: Build and flash ESP32 firmware
2. **Server Startup**: Launch Python Flask application
3. **Network Configuration**: Ensure MQTT broker accessibility
4. **Device Registration**: Verify device authentication and connectivity

#### Production Deployment
1. **Security Review**: Validate encryption keys and authentication
2. **Performance Testing**: Run benchmark tests and load validation
3. **Documentation**: Generate API documentation with Doxygen
4. **Monitoring Setup**: Configure logging and alerting systems

###  Documentation Generation
```bash
# Windows
docs\generate_docs.bat

# Linux/macOS
chmod +x docs/generate_docs.sh
docs/generate_docs.sh

# Manual generation
doxygen docs/Doxyfile

# Documentation output: docs/html/index.html
```

## Configuration Management

### Server Configuration
```json
{
  "poll_period_ms": 10000,
  "upload_period_ms": 20000,
  "buffer_capacity": 256,
  "reg_req_id": 1023
}
```

### Register Selection
The system supports bitwise register selection through the web interface:
- Bit 0: AC Voltage measurement
- Bit 1: AC Current measurement
- Bit 2: Grid Frequency monitoring
- Bit 3-4: PV panel voltage readings
- Bit 5-6: PV panel current readings
- Bit 7: Internal temperature monitoring
- Bit 8: Export power percentage
- Bit 9: Total output power measurement

## API Reference

### Data Endpoints
- `GET /api/data?limit=50` - Retrieve latest telemetry records
- `GET /api/logs/data` - Access data reception logs
- `GET /api/config` - Current device configuration

### Management Endpoints
- `POST /api/config` - Update device configuration
- `POST /api/fota/upload` - Upload firmware for OTA update
- `POST /api/write` - Send register write commands

### Log Endpoints
- `GET /api/logs/fota` - FOTA operation logs
- `GET /api/logs/config` - Configuration change logs
- `GET /api/logs/write` - Register write operation logs

## Security Considerations

### Encryption Implementation
All device communications use military-grade encryption with:
- AES-256-CTR for data confidentiality
- HMAC-SHA256 for message integrity
- Device-specific key derivation
- Protection against replay attacks

### Key Management
- Pre-shared keys configured during device provisioning
- Unique device identifiers for key derivation
- Secure boot sequence validation
- Message sequence number tracking

## Testing and Validation

### Software Testing
The project includes comprehensive testing tools:
- Compression benchmark validation
- MQTT communication testing
- Server upload simulation
- Data integrity verification

## Data Flow Architecture

### Device to Cloud Pipeline
1. ESP32 polls solar inverter via Modbus RTU
2. Data stored in ring buffer with timestamp correlation
3. Periodic compression using delta encoding algorithm
4. Encryption with device-specific keys
5. MQTT transmission to cloud broker
6. Server decryption and decompression
7. Storage in memory with automatic rotation
8. Real-time web dashboard updates

### Cloud to Device Commands
1. Web interface generates configuration or commands
2. Server encrypts payload with device keys
3. MQTT publication to device-specific topics
4. ESP32 decryption and validation
5. Command execution with acknowledgment
6. Status reporting back to server

## Deployment Guidelines

### Production Environment
- Configure MQTT broker with SSL/TLS
- Implement proper firewall rules
- Set up monitoring and alerting
- Regular backup of configuration data

### Scalability Considerations
- Multiple device support with unique identifiers
- Load balancing for high-volume deployments
- Database integration for persistent storage
- Monitoring dashboard for fleet management

## Troubleshooting

### Common Issues
- **Connection Problems**: Verify WiFi credentials and MQTT broker accessibility
- **Data Errors**: Check device PSK configuration and encryption keys
- **Performance Issues**: Adjust polling intervals and buffer capacity
- **FOTA Failures**: Ensure firmware compatibility and stable connection

### Debug Information
The system provides comprehensive logging for:
- Device communication status
- Encryption and decryption operations
- Data compression statistics
- Network connectivity metrics

## Documentation

### API Documentation
Generate complete API documentation using Doxygen:
```bash
# Windows
docs/generate_docs.bat

# Linux/macOS
docs/generate_docs.sh
```

### Code Documentation
All source code includes detailed comments explaining:
- Function parameters and return values
- Algorithm implementation details
- Security considerations
- Performance optimization notes

## License

This project is developed for educational and research purposes. Commercial deployment requires proper licensing and security audit.
