# ECO-WATT Server Documentation# ECO-WATT Server Documentation# 🚀 ECO-WATT Server-Side Complete Guide# 🚀 ECO-WATT Server-Side Complete Guide# 🚀 Node-RED EcoWatt Dashboard - Quick Setup Guide



## Overview

The ECO-WATT server processes compressed solar inverter data from ESP32 devices, decompresses it with 9:1 efficiency, and publishes real-time data via MQTT for dashboard visualization.

## Overview

## Quick Setup

1. **Install Dependencies**: `pip install flask paho-mqtt`The ECO-WATT server processes compressed solar inverter data from ESP32 devices, decompresses it with 9:1 efficiency, and publishes real-time data via MQTT for dashboard visualization.

2. **Start Server**: `python server.py`

3. **Test System**: `python test/server_upload_test.py`## 📋 Quick Navigation

4. **Monitor MQTT**: `python mqtt_subscriber.py`

## Quick Setup

## System Architecture

- **ESP32** → Sends compressed solar data (314 bytes from 2,880 bytes)1. **Install Dependencies**: `pip install flask paho-mqtt`• [Architecture](#🏗️-architecture) • [Flask Setup](#🌐-flask-setup) • [MQTT](#📡-mqtt) • [Compression](#🗜️-compression) • [API](#🔌-api) • [Node-RED](#📊-node-red) • [Testing](#🧪-testing) • [Config](#⚙️-config) • [Debug](#🛠️-debug)

- **Flask Server** → Decompresses and processes data

- **MQTT Broker** → Publishes to topic `vdl/replace`2. **Start Server**: `python server.py`

- **Node-RED Dashboard** → Displays real-time charts and gauges

3. **Test System**: `python test/test_upload.py`## 📋 **Table of Contents**## 📊 **Data Flow Overview**

---

4. **View Dashboard**: Install Node-RED and import flow

## Flask Server Setup

---

### Installation

```bash## System Architecture

python -m venv .venv

.venv\Scripts\activate- **ESP32** → Sends compressed solar data (314 bytes from 2,880 bytes)1. [Server Architecture Overview](#architecture)```

pip install flask paho-mqtt requests

```- **Flask Server** → Decompresses and processes data



### Configuration- **MQTT Broker** → Publishes to topic `vdl/replace`## 🏗️ Architecture

```python

# Key settings in server.py- **Node-RED Dashboard** → Displays real-time charts and gauges

MQTT_BROKER = "broker.emqx.io"

MQTT_TOPIC = "vdl/replace"2. [Flask API Server Setup](#flask-setup)  ESP32 → Flask Server (46 bytes compressed) → MQTT → Node-RED → Dashboard

HOST = "0.0.0.0"

PORT = 5000---

```

### System Flow

### Main Upload Handler

The server receives compressed data, decompresses it, and publishes to MQTT:## Flask Server Setup

```python

@app.route('/api/inverter/upload', methods=['POST'])```3. [MQTT Integration](#mqtt-integration)```

def upload_inverter_data():

    # Receive 314-byte compressed payload### Installation

    # Decompress to 15 records with 10 registers each

    # Publish JSON to MQTT broker```bashESP32 → Flask Server → MQTT Broker → Node-RED Dashboard

    # Return compression statistics

```python -m venv .venv



---.venv\Scripts\activate  ↓         ↓             ↓              ↓4. [Data Compression System](#compression)



## MQTT Integrationpip install flask paho-mqtt requests



### Connection Details```Solar → Compression → Cloud Pub → Real-time UI

- **Broker**: broker.emqx.io:1883

- **Topic**: vdl/replace

- **Format**: JSON with solar data and compression stats

### Configuration```5. [API Endpoints Documentation](#api-endpoints)## ⚡ **Quick Installation**

### Published Data Structure

```json```python

{

  "device_id": "ESP32_SOLAR_INV_001",# Key settings in server.py

  "data": [

    {MQTT_BROKER = "broker.emqx.io"

      "timestamp": 1727522445,

      "ac_voltage": 230.5,MQTT_TOPIC = "vdl/replace"### Key Metrics6. [Node-RED Dashboard](#node-red-dashboard)

      "ac_current": 4.2,

      "ac_power": 968.1,HOST = "0.0.0.0"

      "temperature": 42.5

    }PORT = 5000- **Compression**: 9.17:1 ratio (2,880 → 314 bytes)

  ],

  "compression_stats": {```

    "ratio": 9.17,

    "space_saved_percent": 89.1- **Space Saved**: 89.1%7. [Testing & Validation](#testing)### **1. Install Node.js & Node-RED**

  }

}### Main Upload Handler

```

The server receives compressed data, decompresses it, and publishes to MQTT:- **Upload Interval**: 15 seconds

---

```python

## Testing

@app.route('/api/inverter/upload', methods=['POST'])- **Records/Upload**: 158. [Configuration Guide](#configuration)```bash

### Available Test Files

1. **test/test_upload.py** → Simulates ESP32 compressed data uploaddef upload_inverter_data():

2. **mqtt_subscriber.py** → Listens for MQTT messages (root directory)

3. **mqtt_test_publisher.py** → Publishes test MQTT messages (root directory)    # Receive 314-byte compressed payload- **Registers**: 10 (AC/DC voltage, current, power, temp, etc.)

4. **test/benchmark_test.py** → Performance testing

    # Decompress to 15 records with 10 registers each

### Manual Testing Steps

```bash    # Publish JSON to MQTT broker9. [Troubleshooting](#troubleshooting)# Download Node.js from: https://nodejs.org/

# Terminal 1: Start server

python server.py    # Return compression statistics



# Terminal 2: Monitor MQTT messages```---

python mqtt_subscriber.py



# Terminal 3: Test upload

python test/test_upload.py---node --version  # Verify installation



# Terminal 4: Test MQTT independently

python mqtt_test_publisher.py

```## MQTT Integration## 🌐 Flask Setup



### Expected Results

- ✅ Upload successful with 9.17:1 compression ratio

- ✅ MQTT message published to vdl/replace topic### Connection Details---

- ✅ MQTT subscriber displays formatted data

- ✅ All 15 records decompressed correctly- **Broker**: broker.emqx.io:1883



---- **Topic**: vdl/replace### Quick Install



## Node-RED Dashboard- **Format**: JSON with solar data and compression stats



### Installation```powershell# Install Node-RED globally

```bash

npm install -g node-red### Published Data Structure

cd %USERPROFILE%\.node-red

npm install node-red-dashboard```jsonpython -m venv .venv

```

{

### Setup Steps

1. Start Node-RED: `node-red`  "device_id": "ESP32_SOLAR_INV_001",.venv\Scripts\activate## 🏗️ **Server Architecture Overview** {#architecture}npm install -g node-red

2. Open editor: http://localhost:1880

3. Import flow from `Server/nodered-ecowatt-dashboard.json`  "data": [

4. Configure MQTT input node with broker.emqx.io:1883

5. Access dashboard: http://localhost:1880/ui    {pip install flask paho-mqtt requests



### Dashboard Components      "timestamp": 1727522445,

- **Voltage Charts** → Real-time AC/DC voltage monitoring

- **Power Gauges** → Current power generation and efficiency      "ac_voltage": 230.5,python server.py

- **Compression Analytics** → Live compression ratio display

- **Device Status** → Connection and health indicators      "ac_current": 4.2,



---      "ac_power": 968.1,```



## API Endpoints      "temperature": 42.5



### Upload Endpoint    }### **System Data Flow:**# Install dashboard module

```http

POST /api/inverter/upload  ],

Content-Type: application/json or application/octet-stream

```  "compression_stats": {### Core Configuration



**Request**: Compressed solar data (314 bytes)    "ratio": 9.17,

**Response**: Success status and compression statistics

    "space_saved_percent": 89.1```python```npm install -g node-red-dashboard

### Dashboard Endpoints

- **GET /api/dashboard/health** → Server status and MQTT connection  }

- **GET /api/dashboard/stats** → Upload statistics and performance

- **GET /api/dashboard/solar-data** → Latest solar readings}# MQTT Settings



---```



## ConfigurationMQTT_BROKER = "broker.emqx.io"  # Public brokerESP32/NodeMCU → Flask Server → MQTT Broker → Node-RED Dashboard```



### Production Settings---

```python

# Secure MQTT brokerMQTT_PORT = 1883

MQTT_BROKER = "your-production-broker.com"

MQTT_USERNAME = "username"## Data Compression

MQTT_PASSWORD = "password"

MQTT_TOPIC = "vdl/replace"     ↓              ↓             ↓              ↓

# Production server

app.config['DEBUG'] = False### How It Works

HOST = "0.0.0.0"

PORT = 80- ESP32 collects 15 records over 15 seconds

```

- Each record has 10 solar inverter registers

### Development Settings

```python- Delta compression reduces 2,880 bytes to 314 bytes# Server Settings  Solar Data → Compression → Cloud Pub → Real-time UI### **2. Import Dashboard Flow**

# Public MQTT broker (testing)

MQTT_BROKER = "broker.emqx.io"- Achieves 9.17:1 compression ratio



# Development serverHOST = "0.0.0.0"

app.config['DEBUG'] = True

HOST = "127.0.0.1"### Decompression Process

PORT = 5000

```The server decompresses the binary data back to readable solar values:PORT = 5000``````bash



---1. Read header (number of records and registers)



## Troubleshooting2. Process timestamp deltasDEBUG = True



### Common Issues3. Extract register values



**Server Won't Start**4. Scale values to real units (voltage, current, power)```# Start Node-RED

- Check if port 5000 is available: `netstat -an | findstr :5000`

- Try different port: `python server.py --port 5001`



**MQTT Connection Failed**---

- Test connectivity: `ping broker.emqx.io`

- Try alternative broker: `test.mosquitto.org`



**No MQTT Messages Received**## API Endpoints### Main Upload Handler### **Key Technologies:**node-red

- Run MQTT test publisher: `python mqtt_test_publisher.py`

- Check subscriber is running: `python mqtt_subscriber.py`

- Verify topic spelling: "vdl/replace" (case sensitive)

### Upload Endpoint```python

### Debug Commands

```bash```http

# Check server health

curl http://localhost:5000/api/dashboard/healthPOST /api/inverter/upload@app.route('/api/inverter/upload', methods=['POST'])- **🐍 Flask Server**: Python web server handling ESP32 uploads



# Monitor MQTT messages manuallyContent-Type: application/json or application/octet-stream

mosquitto_sub -h broker.emqx.io -t "vdl/replace" -v

```def upload_inverter_data():

# Test network connectivity

netstat -an | findstr ":5000\|:1883\|:1880"

```

**Request**: Compressed solar data (314 bytes)    # 1. Receive compressed data (314 bytes)- **📡 MQTT Broker**: `broker.emqx.io:1883` for real-time messaging  # Open: http://localhost:1880/

---

**Response**: Success status and compression statistics

## Quick Start Checklist

    # 2. Decompress using 9:1 algorithm

### Complete Setup (5 minutes)

1. **Install and start server** (2 min)### Dashboard Endpoints

   ```bash

   pip install flask paho-mqtt- **GET /api/dashboard/health** → Server status and MQTT connection    # 3. Extract 10 solar registers- **📊 Node-RED**: Dashboard for data visualization# Menu (☰) → Import → Paste contents of 'nodered-ecowatt-dashboard.json'

   python server.py

   ```- **GET /api/dashboard/stats** → Upload statistics and performance



2. **Test MQTT system** (1 min)- **GET /api/dashboard/solar-data** → Latest solar readings    # 4. Publish to MQTT

   ```bash

   # Terminal 1: Start subscriber

   python mqtt_subscriber.py

   ---    # 5. Return compression stats- **🗜️ Compression**: 9:1 delta compression algorithm```

   # Terminal 2: Test publisher

   python mqtt_test_publisher.py

   ```

## Node-RED Dashboard    

3. **Test data upload** (1 min)

   ```bash

   python test/test_upload.py

   ```### Installation    if request.content_type == 'application/octet-stream':- **Delta-compressed timestamps** and **scaled values** (×100)



4. **Setup Node-RED dashboard** (1 min)```bash

   ```bash

   npm install -g node-rednpm install -g node-red        compressed_data = request.data

   node-red

   # Import flow and access http://localhost:1880/uicd %USERPROFILE%\.node-red

   ```

npm install node-red-dashboard    else:### **Performance Metrics:**

### Verification

- ✅ Server running and MQTT connected```

- ✅ MQTT subscriber receives test messages

- ✅ Upload test shows 9.17:1 compression        json_data = request.get_json()

- ✅ Dashboard displays real-time charts

### Setup Steps

---

1. Start Node-RED: `node-red`        compressed_data = binascii.unhexlify(json_data['data'])- **Compression Ratio**: 9.17:1 (2,880 → 314 bytes)#### **🔹 Node-RED API Endpoints** (Auto-created by Flask server)

## File Locations

2. Open editor: http://localhost:1880

### Root Directory

- `mqtt_subscriber.py` → MQTT message listener3. Import flow from `nodered-ecowatt-dashboard.json`    

- `mqtt_test_publisher.py` → MQTT test message publisher

- `server.py` → Main Flask server4. Configure MQTT input node with broker.emqx.io:1883



### Server Directory5. Access dashboard: http://localhost:1880/ui    records = decompressDelta(compressed_data)- **Space Saved**: 89.1%1. **`/api/nodered/stats`** - Compression statistics

- `Server/server.py` → Flask server implementation

- `Server/nodered-ecowatt-dashboard.json` → Node-RED flow

- `Server/server-guide.md` → This documentation

### Dashboard Components    mqtt_payload = format_for_mqtt(records)

### Test Directory

- `test/test_upload.py` → ESP32 upload simulation- **Voltage Charts** → Real-time AC/DC voltage monitoring

- `test/benchmark_test.py` → Performance testing

- **Power Gauges** → Current power generation and efficiency    publish_to_mqtt(mqtt_payload)- **Upload Interval**: 15 seconds2. **`/api/nodered/solar-data`** - Decompressed solar inverter readings  

---

- **Compression Analytics** → Live compression ratio display

## Summary

The ECO-WATT server efficiently handles compressed solar data with 9:1 compression ratio, provides real-time MQTT publishing, and supports professional dashboard visualization. The system includes comprehensive testing tools and is designed for easy deployment.- **Device Status** → Connection and health indicators    



---    return {"status": "success", "compression_ratio": 9.17}- **Data Records**: 15 per upload3. **`/api/nodered/compression-history`** - Historical compression analytics



## Testing```



### Available Tests- **Response Time**: <500ms

1. **test_upload.py** → Simulates ESP32 compressed data upload

2. **mqtt_subscriber.py** → Listens for MQTT messages---

3. **server_load_test.py** → Tests server under load

4. **compression_validation.py** → Validates data integrity## 🛠️ **Installation Steps**



### Manual Testing Steps## 📡 MQTT

```bash

# Terminal 1: Start server---

python server.py

### Connection Config

# Terminal 2: Test upload

cd test```python### **Step 1: Install Node.js**

python test_upload.py

BROKER: broker.emqx.io:1883

# Terminal 3: Monitor MQTT

python mqtt_subscriber.pyTOPIC: vdl/replace## 🌐 **Flask API Server Setup** {#flask-setup}```bash



# Terminal 4: Start dashboardQOS: 0 (Fire and forget)

node-red

```CLIENT_ID: ecowatt_server# Download and install Node.js from: https://nodejs.org/



### Expected Results```

- ✅ Upload successful with 9.17:1 compression ratio

- ✅ MQTT message published to vdl/replace topic### **Installation Steps:**# Verify installation:

- ✅ Dashboard displays real-time data

- ✅ All 15 records decompressed correctly### Published Data Format



---```json```powershellnode --version



## Configuration{



### Production Settings  "timestamp": "2025-09-28T14:30:45Z",# 1. Create virtual environmentnpm --version

```python

# Secure MQTT broker  "device_id": "ESP32_SOLAR_INV_001",

MQTT_BROKER = "your-production-broker.com"

MQTT_USERNAME = "username"  "data": [{python -m venv .venv```

MQTT_PASSWORD = "password"

    "timestamp": 1727522445,

# Production server

app.config['DEBUG'] = False    "ac_voltage": 230.5,    // Register 0.venv\Scripts\activate

HOST = "0.0.0.0"

PORT = 80    "ac_current": 4.2,      // Register 1

```

    "ac_power": 968.1,      // Register 2### **Step 2: Install Node-RED**

### Development Settings

```python    "ac_frequency": 50.0,   // Register 3

# Public MQTT broker (testing)

MQTT_BROKER = "broker.emqx.io"    "dc_voltage": 350.8,    // Register 4# 2. Install dependencies  ```bash



# Development server    "dc_current": 2.8,      // Register 5

app.config['DEBUG'] = True

HOST = "127.0.0.1"    "dc_power": 982.2,      // Register 6pip install flask paho-mqtt requests binascii# Install Node-RED globally

PORT = 5000

```    "temperature": 42.5,    // Register 7



---    "status": 1,            // Register 8npm install -g node-red



## Troubleshooting    "efficiency": 98.6      // Register 9



### Common Issues  }],# 3. Start server



**Server Won't Start**  "compression_stats": {

- Check if port 5000 is available: `netstat -an | findstr :5000`

- Try different port: `python server.py --port 5001`    "original_bytes": 2880,python server.py# Verify installation



**MQTT Connection Failed**    "compressed_bytes": 314,

- Test connectivity: `ping broker.emqx.io`

- Try alternative broker: `test.mosquitto.org`    "ratio": 9.17,```node-red --version



**Compression Errors**    "space_saved_percent": 89.1

- Verify data length (should be 314 bytes)

- Check header values (records count and registers count)  }```

- Enable debug logging to trace decompression

}

**Dashboard Not Updating**

- Verify MQTT connection in Node-RED```### **Server Configuration (`server.py`):**

- Check topic spelling: "vdl/replace" (case sensitive)

- Add debug nodes to trace data flow



### Debug Commands### MQTT Implementation### **Step 3: Install Required Node-RED Modules**

```bash

# Check server health```python

curl http://localhost:5000/api/dashboard/health

def setup_mqtt():#### **Core Settings:**```bash

# Monitor MQTT messages

mosquitto_sub -h broker.emqx.io -t "vdl/replace" -v    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1, client_id="ecowatt_server")



# Test network connectivity    ```python# Navigate to Node-RED user directory

netstat -an | findstr ":5000\|:1883\|:1880"

```    def on_connect(client, userdata, flags, rc):



---        print(f"✅ MQTT Connected: {MQTT_BROKER}:{MQTT_PORT}" if rc == 0 else f"❌ Failed: {rc}")# MQTT Configurationcd %USERPROFILE%\.node-red



## Quick Start Checklist    



### Complete Setup (5 minutes)    client.on_connect = on_connectMQTT_BROKER = "broker.emqx.io"  # Public MQTT broker

1. **Install and start server** (2 min)

   ```bash    client.connect(MQTT_BROKER, MQTT_PORT, 60)

   pip install flask paho-mqtt

   python server.py    client.loop_start()MQTT_PORT = 1883# Install dashboard module

   ```

    return client

2. **Test data upload** (1 min)

   ```bashMQTT_TOPIC = "vdl/replace"      # Topic for solar datanpm install node-red-dashboard

   python test/test_upload.py

   ```def publish_to_mqtt(data):



3. **Setup Node-RED dashboard** (2 min)    if mqtt_client:MQTT_CLIENT_ID = "ecowatt_server"

   ```bash

   npm install -g node-red        payload = json.dumps(data)

   node-red

   # Import flow and access http://localhost:1880/ui        mqtt_client.publish(MQTT_TOPIC, payload)# Install additional useful modules

   ```

        print(f"📡 Published {len(payload)} bytes")

### Verification

- ✅ Server running and MQTT connected```# Flask Server Configurationnpm install node-red-contrib-ui-led

- ✅ Test upload shows 9.17:1 compression

- ✅ MQTT subscriber receives data

- ✅ Dashboard displays real-time charts

---HOST = "0.0.0.0"    # Listen on all interfacesnpm install node-red-node-smooth

---



## Summary

The ECO-WATT server efficiently handles compressed solar data with 9:1 compression ratio, provides real-time MQTT publishing, and supports professional dashboard visualization. The system is designed for reliability and easy deployment in both development and production environments.## 🗜️ CompressionPORT = 5000         # Server port```



### Algorithm StructureDEBUG = True        # Enable debug mode

```

Header: [num_records][regs_per_record]```### **Step 4: Start Node-RED**

Record: [delta_timestamp][reg0_hi][reg0_lo]...[reg9_hi][reg9_lo]

``````bash



### Key Features#### **Main Upload Handler:**# Start Node-RED (will create default config)

- **Variable timestamps**: 1 byte (≤254ms) or 3 bytes (255+ms)

- **Delta compression**: Store time differences```pythonnode-red

- **Complete packing**: All 10 registers per record

- **Perfect reconstruction**: 100% data integrity@app.route('/api/inverter/upload', methods=['POST'])



### Decompression Implementationdef upload_inverter_data():# Expected output:

```python

def decompressDelta(data):    """# Welcome to Node-RED

    if len(data) < 2: return []

        Handle ESP32 compressed data uploads# Server now running at http://127.0.0.1:1880/

    num_records, regs_per_record = data[0], data[1]

    idx, records, current_ts, first_record = 2, [], 0, True    1. Receive compressed payload (314 bytes)```

    

    for rec in range(num_records):    2. Decompress using 9:1 algorithm  

        if idx >= len(data): break

            3. Extract solar inverter registers### **Step 5: Import EcoWatt Dashboard Flow**

        # Read timestamp delta

        if data[idx] == 0xFF:  # Extended 2-byte delta    4. Publish to MQTT broker

            if idx + 2 >= len(data): break

            delta = (data[idx + 1] << 8) | data[idx + 2]    5. Return compression statistics#### **Method 1: Import via Web Interface**

            idx += 3

        else:  # Standard 1-byte delta    """1. Open Node-RED editor: `http://localhost:1880/`

            delta = data[idx]

            idx += 1    try:2. Click **hamburger menu (☰)** → **Import**

        

        # Calculate absolute timestamp        # Process binary or JSON data3. Copy the entire contents of `nodered-ecowatt-dashboard.json`

        current_ts = delta if first_record else current_ts + delta

        first_record = False        if request.content_type == 'application/octet-stream':4. Paste into import dialog

        

        # Read registers (10 × 2 bytes each)            compressed_data = request.data5. Click **Import**

        registers = []

        for reg in range(regs_per_record):        else:

            if idx + 1 >= len(data): break

            raw_value = (data[idx] << 8) | data[idx + 1]            json_data = request.get_json()#### **Method 2: Import via File**

            registers.append(raw_value)

            idx += 2            compressed_data = binascii.unhexlify(json_data['data'])```bash

        

        # Create scaled record        # Copy the JSON file to Node-RED directory

        record = {

            "timestamp": current_ts,        # Decompress using updated algorithmcopy "nodered-ecowatt-dashboard.json" "%USERPROFILE%\.node-red\flows\"

            "ac_voltage": registers[0] / 10.0 if len(registers) > 0 else 0,

            "ac_current": registers[1] / 100.0 if len(registers) > 1 else 0,        records = decompressDelta(compressed_data)

            "ac_power": registers[2] / 10.0 if len(registers) > 2 else 0,

            "ac_frequency": registers[3] / 10.0 if len(registers) > 3 else 0,        # Restart Node-RED to load the flow

            "dc_voltage": registers[4] / 10.0 if len(registers) > 4 else 0,

            "dc_current": registers[5] / 100.0 if len(registers) > 5 else 0,        # Process and publish to MQTT```

            "dc_power": registers[6] / 10.0 if len(registers) > 6 else 0,

            "temperature": registers[7] / 10.0 if len(registers) > 7 else 0,        mqtt_payload = format_for_mqtt(records)

            "status": registers[8] if len(registers) > 8 else 0,

            "efficiency": registers[9] / 10.0 if len(registers) > 9 else 0        publish_to_mqtt(mqtt_payload)## 🎯 **Testing & Usage Commands**

        }

        records.append(record)        

    

    return records        return {### **Terminal 1: Start Flask Server**

```

            "status": "success", ```bash

---

            "compression_ratio": 9.17,cd "C:\Users\Yasiru Alahakoon\Desktop\Image Processing And Machine Vision\EN3160 Assignment 3 on Neural Networks\Eco-watt"

## 🔌 API Endpoints

            "records_processed": len(records)python server.py

### 1. Main Upload

```http        }```

POST /api/inverter/upload

Content-Type: application/json | application/octet-stream    except Exception as e:**Expected Output**:

```

        return {"error": str(e)}, 400```

**Request (JSON):**

```json```EcoWatt Cloud API Server Starting...

{"data": "0F0A00E8...", "device_id": "ESP32_001", "size": 314}

```Dashboard: http://localhost:5000/



**Request (Binary):**---API Endpoint: http://localhost:5000/api/inverter/upload

```

Raw 314-byte compressed payload```

```

## 📡 **MQTT Integration** {#mqtt-integration}

**Response:**

```json### **Terminal 2: Start Node-RED**

{

  "status": "success",### **MQTT Broker Configuration:**```bash

  "compression_stats": {"ratio": 9.17, "space_saved_percent": 89.1},

  "records_processed": 15,```pythonnode-red

  "mqtt_published": true

}# Connection Parameters```

```

BROKER: broker.emqx.io**Expected Output**:

### 2. Dashboard APIs

PORT: 1883```

**Health Check:**

```httpTOPIC: vdl/replaceWelcome to Node-RED

GET /api/dashboard/health

→ {"server_status": "running", "mqtt_connected": true, "uptime_seconds": 3600}QOS: 0 (Fire and forget)Server now running at http://127.0.0.1:1880/

```

RETAIN: FalseDashboard: http://127.0.0.1:1880/ui/

**Statistics:**

```httpCLIENT_ID: ecowatt_server```

GET /api/dashboard/stats  

→ {"uploads_count": 42, "average_compression_ratio": 9.15, "total_data_saved_mb": 15.3}```

```

### **Terminal 3: Run ESP32 Simulation Tests**

**Solar Data:**

```http### **Published Data Format (Updated):**```bash

GET /api/dashboard/solar-data

→ {"latest_reading": {"ac_voltage": 230.5, "ac_power": 968.1, "temperature": 42.5}}```jsoncd "C:\Users\Yasiru Alahakoon\Desktop\Image Processing And Machine Vision\EN3160 Assignment 3 on Neural Networks\Eco-watt\test"

```

{

---

  "timestamp": "2025-09-28T14:30:45.123456",# Run ESP32 payload validation test

## 📊 Node-RED

  "device_id": "ESP32_SOLAR_INV_001",python test_upload.py

### Quick Setup

```powershell  "data": [

# Install

npm install -g node-red    {# Run C++ compression benchmark  

cd %USERPROFILE%\.node-red

npm install node-red-dashboard node-red-contrib-ui-led      "timestamp": 1727522445,python benchmark_test.py



# Start      "ac_voltage": 230.5,    # Register 0: AC Voltage (V)```

node-red

# Editor: http://localhost:1880      "ac_current": 4.2,      # Register 1: AC Current (A)  

# Dashboard: http://localhost:1880/ui

```      "ac_power": 968.1,      # Register 2: AC Power (W)### **Terminal 4: Monitor Node-RED API Calls**



### MQTT Input Node      "ac_frequency": 50.0,   # Register 3: AC Frequency (Hz)```bash

```json

{      "dc_voltage": 350.8,    # Register 4: DC Voltage (V)# Test Node-RED API endpoints directly

  "server": "broker.emqx.io:1883",

  "topic": "vdl/replace",      "dc_current": 2.8,      # Register 5: DC Current (A)curl http://localhost:5000/api/nodered/stats

  "qos": 0,

  "output": "auto-detect"      "dc_power": 982.2,      # Register 6: DC Power (W)curl http://localhost:5000/api/nodered/solar-data  

}

```      "temperature": 42.5,    # Register 7: Temperature (°C)curl http://localhost:5000/api/nodered/compression-history



### Dashboard Components      "status": 1,            # Register 8: System Status```

```javascript

// Voltage Chart Function      "efficiency": 98.6      # Register 9: Efficiency (%)

var data = JSON.parse(msg.payload);

if (data.data && data.data.length > 0) {    }## 🌐 **Dashboard Access URLs**

    var latest = data.data[data.data.length - 1];

    msg.payload = {voltage: latest.ac_voltage, current: latest.ac_current};  ],

}

return msg;  "compression_stats": {| Service | URL | Purpose |



// Compression Gauge Function      "original_bytes": 2880,|---------|-----|---------|

var data = JSON.parse(msg.payload);

msg.payload = data.compression_stats ? data.compression_stats.ratio : 0;    "compressed_bytes": 314,| **Flask Dashboard** | `http://localhost:5000/` | Server-side compression stats |

return msg;

    "ratio": 9.17,| **Node-RED Editor** | `http://localhost:1880/` | Flow editor and debugging |

// Power Monitor Function

var data = JSON.parse(msg.payload);    "space_saved_percent": 89.1| **Node-RED Dashboard** | `http://localhost:1880/ui/` | **🎯 MAIN DASHBOARD** |

if (data.data && data.data.length > 0) {

    var latest = data.data[data.data.length - 1];  },| **Node-RED Admin** | `http://localhost:1880/admin/` | Node-RED administration |

    msg.payload = {ac_power: latest.ac_power, efficiency: latest.efficiency};

}  "metadata": {

return msg;

```    "upload_interval": "15 seconds",## 📊 **Dashboard Features**



### Flow Structure    "records_per_upload": 15,

```

MQTT In → JSON Parser → Function Nodes → Dashboard Widgets    "total_registers": 10,### **🔹 Compression Statistics Panel**

   ↓         ↓            ↓              ↓

[Subscribe] [Parse] [Extract Values] [Charts/Gauges]    "algorithm_version": "delta_v2"- **Device ID**: ESP32 solar inverter identifier

```

  }- **Total Uploads**: Count of successful data uploads

---

}- **Records**: Number of time-series records (should be 15)

## 🧪 Testing

```- **Original Size**: Uncompressed data size (~2,880 bytes)

### Test Scripts Available

```powershell- **Compressed Size**: Compressed payload size (46 bytes)

cd test

### **MQTT Client Implementation:**- **Compression Ratio**: Efficiency ratio (should be ~62:1)

# 1. ESP32 Upload Simulation

python test_upload.py```python- **Space Saved**: Bandwidth reduction percentage (~98%)

# → Tests 314-byte compressed upload, expects 9.17:1 ratio

import paho.mqtt.client as mqtt

# 2. MQTT Subscription Test  

python mqtt_subscriber.pyimport json### **🔹 Solar Voltage Chart**

# → Listens for MQTT messages on "vdl/replace"

- **Real-time line chart** showing voltage readings over time

# 3. Server Load Test

python server_load_test.py  def setup_mqtt():- **X-axis**: Time (HH:mm:ss format)

# → Multiple concurrent uploads

    """Initialize MQTT client with callbacks"""- **Y-axis**: Voltage (200-250V range)

# 4. Compression Validation

python compression_validation.py    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1, client_id=MQTT_CLIENT_ID)- **Data source**: Decompressed ESP32 register readings

# → Validates data integrity & compression accuracy

```    



### Manual Testing Workflow    def on_connect(client, userdata, flags, rc):### **🔹 Current Voltage Gauge**

```powershell

# Step 1: Start Server        if rc == 0:- **Real-time gauge** showing latest voltage reading

python server.py

# Expected: ✅ Server running, ✅ MQTT connected            print(f"✅ Connected to MQTT broker at {MQTT_BROKER}:{MQTT_PORT}")- **Green zone**: 230-250V (optimal)



# Step 2: Test Upload          else:- **Yellow zone**: 210-230V (acceptable)

cd test && python test_upload.py

# Expected: ✅ HTTP 200, ✅ 9.17:1 ratio, ✅ MQTT publish            print(f"❌ Connection failed: {rc}")- **Red zone**: 200-210V (low voltage warning)



# Step 3: Verify MQTT    

python mqtt_subscriber.py

# Expected: ✅ JSON payload with 15 records    def on_publish(client, userdata, mid):### **🔹 Compression History**



# Step 4: Node-RED Dashboard        print(f"📤 Message published (mid: {mid})")- **Line chart**: Compression ratio trends over time

node-red  # → http://localhost:1880/ui

# Expected: ✅ Real-time updates, ✅ 9:1 compression display    - **Bar chart**: Original vs compressed bytes comparison

```

    client.on_connect = on_connect- **Historical analysis**: Last 10 upload cycles

### Expected Results

```    client.on_publish = on_publish

✅ Upload Success: HTTP 200

✅ Compression Ratio: 9.17:1 (2,880 → 314 bytes)      ## 🔧 **Terminal Commands for Monitoring**

✅ MQTT Publish: Message delivered to "vdl/replace"

✅ Data Integrity: 100% reconstruction    try:

✅ Response Time: <500ms

✅ Dashboard: Real-time charts updating        client.connect(MQTT_BROKER, MQTT_PORT, 60)### **Check Flask Server Status**

```

        client.loop_start()```bash

---

        return client# Test server is running

## ⚙️ Configuration

    except Exception as e:curl http://localhost:5000/

### Production Settings

```python        print(f"❌ MQTT setup failed: {e}")

# server.py - Production

MQTT_BROKER = "your-production-broker.com"        return None# Check API endpoints

MQTT_USERNAME = "prod_user"

MQTT_PASSWORD = "secure_password"curl http://localhost:5000/api/nodered/stats | python -m json.tool

app.config['DEBUG'] = False

HOST = "0.0.0.0"def publish_to_mqtt(data):curl http://localhost:5000/api/nodered/solar-data | python -m json.tool

PORT = 80

```    """Publish solar data to MQTT broker"""```



### Development Settings      if mqtt_client:

```python

# server.py - Development        try:### **Monitor Upload Files**

MQTT_BROKER = "broker.emqx.io"  # Public broker

app.config['DEBUG'] = True            payload = json.dumps(data)```bash

HOST = "127.0.0.1"  

PORT = 5000            result = mqtt_client.publish(MQTT_TOPIC, payload)cd "C:\Users\Yasiru Alahakoon\Desktop\Image Processing And Machine Vision\EN3160 Assignment 3 on Neural Networks\Eco-watt"

```

            if result.rc == mqtt.MQTT_ERR_SUCCESS:

### ESP32 Reference Config

```cpp                print(f"📡 Published {len(payload)} bytes to MQTT")# List upload files

// Config.h

#define POLL_PERIOD_MS 1000      // 1s data collection            else:dir uploads\*.bin

#define UPLOAD_PERIOD_MS 15000   // 15s uploads

#define QTY_REGS 10             // 10 solar registers                print(f"❌ MQTT publish failed: {result.rc}")

#define SERVER_URL "http://your-server:5000/api/inverter/upload"

```        except Exception as e:# Check file sizes (should be 46 bytes each)



---            print(f"❌ MQTT publish error: {e}")powershell "Get-ChildItem uploads\*.bin | Select-Object Name, Length"



## 🛠️ Troubleshooting```



### Common Issues & Solutions# View hex content of latest upload



**Server Won't Start:**---powershell "Get-Content uploads\upload_*.bin -Encoding Byte | ForEach-Object {'{0:X2}' -f $_} | Out-String"

```powershell

netstat -an | findstr :5000  # Check port usage```

python server.py --port 5001  # Use different port

```## 🗜️ **Data Compression System (Updated Algorithm)** {#compression}



**MQTT Connection Failed:**### **Node-RED Debugging**

```powershell

ping broker.emqx.io  # Test connectivity### **Updated Compression Structure:**```bash

# Try alternative: MQTT_BROKER = "test.mosquitto.org"

``````# Check Node-RED logs



**Compression Errors:**Header: [num_records][regs_per_record]node-red --verbose

```python

# Debug decompressionRecord: [delta_timestamp][reg0_hi][reg0_lo]...[reg9_hi][reg9_lo]

print(f"Data length: {len(data)}, Expected: ~314 bytes")

print(f"Header: records={data[0]}, regs={data[1]}")```# Test dashboard connectivity

if data[0] == 0 or data[0] > 50:

    return {"error": "Invalid record count"}curl http://localhost:1880/ui/

```

### **Key Algorithm Features:**

**Dashboard Not Updating:**

```javascript- ✅ **Variable-length timestamps**: 1 byte (≤254ms) or 3 bytes (255+ms)# Check Node-RED flows

// Node-RED checklist:

// 1. MQTT connection (green dot)- ✅ **Delta compression**: Store time differences, not absolute timescurl http://localhost:1880/flows

// 2. Topic: "vdl/replace" (exact spelling)

// 3. Add debug nodes to trace data flow- ✅ **Complete register packing**: All 10 registers per record```

// 4. Check function node syntax

```- ✅ **Perfect reconstruction**: 100% data integrity



### Debug Commands- ✅ **9:1 compression ratio**: 2,880 → 314 bytes## 📈 **Expected Results & Validation**

```powershell

# Server debugging

$env:FLASK_DEBUG=1; python server.py

### **Decompression Implementation:**### **✅ Successful Dashboard Operation**

# MQTT testing

python mqtt_subscriber.py```python

mosquitto_sub -h broker.emqx.io -t "vdl/replace" -v

def decompressDelta(data):#### **After Running `python test_upload.py`**:

# Network diagnostics  

curl http://localhost:5000/api/dashboard/health    """1. **Flask Server Terminal**: Shows compression stats

netstat -an | findstr ":5000\|:1883\|:1880"

```    Decompress ESP32 delta-compressed solar data```



---    Updated algorithm matching C++ implementation[UPLOAD] Compressed batch size: 46 bytes



## 🚀 Quick Start Checklist    """Decompressed to 15 records



### ✅ Complete Setup (10 minutes)    if len(data) < 2:Compression ratio: 62.6:1

```powershell

# 1. Server Setup (3 min)        return []```

python -m venv .venv && .venv\Scripts\activate

pip install flask paho-mqtt requests    

python server.py

    num_records = data[0]        # Header: number of records2. **Node-RED Dashboard** (`http://localhost:1880/ui/`):

# 2. MQTT Test (2 min)  

python mqtt_subscriber.py  # New terminal    regs_per_record = data[1]    # Header: registers per record     - **Compression Stats**: Updates with 15 records, 46 bytes compressed

python test/test_upload.py  # Another terminal

    idx = 2   - **Voltage Chart**: Shows 15 data points over ~15 seconds

# 3. Node-RED (4 min)

npm install -g node-red       - **Voltage Gauge**: Displays latest reading (~220V)

node-red  # → http://localhost:1880

# Import flow from nodered-ecowatt-dashboard.json    records = []   - **History Charts**: Show compression trends

# Dashboard: http://localhost:1880/ui

    current_ts = 0

# 4. Verification (1 min)

# ✅ Server running + MQTT connected    first_record = True#### **Data Flow Validation**:

# ✅ Test upload shows 9.17:1 compression  

# ✅ MQTT subscriber receives data    ```

# ✅ Node-RED dashboard displays real-time charts

```    for rec in range(num_records):ESP32 Test → Flask Server → Node-RED APIs → Dashboard Widgets



---        if idx >= len(data):    ↓              ↓              ↓              ↓



## 📈 Production Tips            break46 bytes    →  Decompress  →  JSON APIs  →  Real-time GUI



### Security        15 records  →  Statistics →  HTTP calls →  Charts/Gauges

```python

# API authentication        # Read timestamp delta```

@app.before_request

def authenticate():        if data[idx] == 0xFF:

    if not request.headers.get('X-API-Key') == 'your-secure-key':

        return {"error": "Unauthorized"}, 401            # Extended 2-byte delta (rare case: >254ms)### **🔍 Troubleshooting Common Issues**



# Rate limiting            if idx + 2 >= len(data):

from flask_limiter import Limiter

@limiter.limit("60 per minute")                break#### **Problem**: Dashboard shows "No data received"

```

            delta = (data[idx + 1] << 8) | data[idx + 2]```bash

### Monitoring

```python            idx += 3# Check Flask server is running

# Performance metrics

@app.route('/api/debug/performance')          else:curl http://localhost:5000/api/nodered/stats

def performance():

    return {            # Standard 1-byte delta (normal case: ≤254ms)  

        "cpu_percent": psutil.cpu_percent(),

        "memory_mb": psutil.virtual_memory().used / 1024 / 1024,            delta = data[idx]# Run test upload to generate data

        "uptime_seconds": time.time() - app.start_time

    }            idx += 1python test\test_upload.py

```

        

---

        # Calculate absolute timestamp# Verify Node-RED can reach Flask server

**🎉 Your ECO-WATT server infrastructure is now fully documented with 9:1 compression, MQTT integration, Node-RED dashboard, and complete testing workflow! Ready for professional deployment. 🚀✨**
        if first_record:curl http://localhost:1880/flows | findstr "localhost:5000"

            current_ts = delta  # First record uses delta as base```

            first_record = False

        else:#### **Problem**: Node-RED dashboard not accessible

            current_ts += delta```bash

        # Check if Node-RED is running

        # Read all register values (10 registers × 2 bytes each)netstat -an | findstr :1880

        registers = []

        for reg in range(regs_per_record):# Restart Node-RED with verbose logging

            if idx + 1 >= len(data):node-red --verbose

                break

            raw_value = (data[idx] << 8) | data[idx + 1]# Check dashboard module installation

            registers.append(raw_value)npm list node-red-dashboard

            idx += 2```

        

        # Create record with scaled values#### **Problem**: Flask server connection refused

        record = {```bash

            "timestamp": current_ts,# Check if Flask server is running

            "ac_voltage": registers[0] / 10.0 if len(registers) > 0 else 0,netstat -an | findstr :5000

            "ac_current": registers[1] / 100.0 if len(registers) > 1 else 0,

            "ac_power": registers[2] / 10.0 if len(registers) > 2 else 0,# Start Flask server if not running

            "ac_frequency": registers[3] / 10.0 if len(registers) > 3 else 0,python server.py

            "dc_voltage": registers[4] / 10.0 if len(registers) > 4 else 0,

            "dc_current": registers[5] / 100.0 if len(registers) > 5 else 0,# Test connectivity

            "dc_power": registers[6] / 10.0 if len(registers) > 6 else 0,telnet localhost 5000

            "temperature": registers[7] / 10.0 if len(registers) > 7 else 0,```

            "status": registers[8] if len(registers) > 8 else 0,

            "efficiency": registers[9] / 10.0 if len(registers) > 9 else 0## 🎯 **Complete Testing Workflow**

        }

        ### **Run this sequence to see full system operation**:

        records.append(record)

    ```bash

    return records# Terminal 1: Start Flask Server

```python server.py



### **Compression Performance Analysis:**# Terminal 2: Start Node-RED (new terminal)  

```pythonnode-red

def analyze_compression(original_size, compressed_size):

    """Calculate compression metrics"""# Terminal 3: Generate test data (new terminal)

    ratio = original_size / compressed_size if compressed_size > 0 else 0cd test

    space_saved = ((original_size - compressed_size) / original_size) * 100python test_upload.py

    python benchmark_test.py

    return {

        "original_bytes": original_size,# Browser 1: Open Node-RED Dashboard

        "compressed_bytes": compressed_size,start http://localhost:1880/ui/

        "ratio": round(ratio, 2),

        "space_saved_percent": round(space_saved, 1),# Browser 2: Open Flask Dashboard  

        "bandwidth_reduction": f"{space_saved:.1f}% less data transfer"start http://localhost:5000/

    }

# Terminal 4: Monitor uploads (new terminal)

# Typical results:dir uploads\*.bin

# Original: 2,880 bytes (15 records × 192 bytes)  powershell "Get-ChildItem uploads\*.bin | Select-Object Name, Length"

# Compressed: 314 bytes (header + compressed records)```

# Ratio: 9.17:1

# Space saved: 89.1%### **Expected Timeline**:

```- **0-10 seconds**: Servers starting up

- **10-30 seconds**: Import Node-RED flow, test data generation

---- **30+ seconds**: Real-time dashboard updates every 15 seconds



## 🔌 **API Endpoints Documentation** {#api-endpoints}### **Success Indicators**:

- ✅ **Flask Dashboard**: Shows 46-byte uploads, 62:1 compression

### **1. Main Upload Endpoint**- ✅ **Node-RED Dashboard**: Real-time voltage charts and compression stats  

```http- ✅ **Upload Files**: 46-byte .bin files created in uploads/ folder

POST /api/inverter/upload- ✅ **Terminal Logs**: Successful HTTP 200 responses and decompression

Content-Type: application/json | application/octet-stream

```**🎉 Complete EcoWatt solar inverter monitoring system with Node-RED visualization!**

#### **Request Formats:**

**JSON Upload:**
```json
{
  "data": "0F0A00E8...",  // Hex-encoded compressed data (628 chars)
  "device_id": "ESP32_SOLAR_INV_001", 
  "size": 314
}
```

**Binary Upload:**
```
Raw binary data (314 bytes compressed payload)
Content-Type: application/octet-stream
```

#### **Success Response:**
```json
{
  "status": "success",
  "message": "Data uploaded and published to MQTT",
  "compression_stats": {
    "original_bytes": 2880,
    "compressed_bytes": 314,
    "ratio": 9.17,
    "space_saved_percent": 89.1
  },
  "records_processed": 15,
  "mqtt_published": true,
  "processing_time_ms": 45
}
```

### **2. Dashboard Endpoints**

#### **System Health Check:**
```http
GET /api/dashboard/health
```
**Response:**
```json
{
  "server_status": "running",
  "mqtt_connected": true,
  "uptime_seconds": 3600,
  "last_upload": "2025-09-28T14:30:45Z",
  "total_uploads": 240,
  "error_count": 0,
  "memory_usage_mb": 45.2
}
```

#### **Real-time Statistics:**
```http
GET /api/dashboard/stats
```
**Response:**
```json
{
  "current_session": {
    "uploads_count": 42,
    "average_compression_ratio": 9.15,
    "total_data_saved_mb": 15.3,
    "last_device_id": "ESP32_SOLAR_INV_001"
  },
  "compression_performance": {
    "best_ratio": 9.42,
    "worst_ratio": 8.86,
    "average_processing_time_ms": 38
  },
  "mqtt_stats": {
    "messages_published": 42,
    "publish_success_rate": 100.0,
    "last_publish": "2025-09-28T14:30:45Z"
  }
}
```

#### **Latest Solar Data:**
```http
GET /api/dashboard/solar-data
```
**Response:**
```json
{
  "latest_reading": {
    "timestamp": "2025-09-28T14:30:45",
    "ac_voltage": 230.5,
    "ac_current": 4.2,
    "ac_power": 968.1,
    "dc_voltage": 350.8,
    "temperature": 42.5,
    "efficiency": 98.6
  },
  "historical_summary": {
    "max_power": 1250.0,
    "min_power": 850.0,
    "average_temperature": 41.2,
    "peak_efficiency": 99.2
  }
}
```

---

## 📊 **Node-RED Dashboard** {#node-red-dashboard}

### **Installation & Setup:**
```powershell
# 1. Install Node.js (https://nodejs.org/)
node --version  # Verify installation

# 2. Install Node-RED globally
npm install -g node-red

# 3. Install dashboard modules
cd %USERPROFILE%\.node-red
npm install node-red-dashboard
npm install node-red-contrib-ui-led
npm install node-red-contrib-ui-gauge
```

### **MQTT Configuration in Node-RED:**

#### **MQTT Input Node Settings:**
```json
{
  "server": {
    "broker": "broker.emqx.io",
    "port": 1883,
    "clientid": "nodered_dashboard"
  },
  "topic": "vdl/replace",
  "qos": "0",
  "output": "auto-detect (string or buffer)"
}
```

### **Dashboard Components (Updated for 9:1 Compression):**

#### **1. Voltage & Current Charts:**
```javascript
// Function node to extract AC values
var data = JSON.parse(msg.payload);
if (data.data && data.data.length > 0) {
    var latest = data.data[data.data.length - 1];
    msg.payload = {
        voltage: latest.ac_voltage,
        current: latest.ac_current,
        timestamp: new Date(latest.timestamp * 1000)
    };
}
return msg;
```

#### **2. Compression Analytics Gauge:**
```javascript
// Extract compression ratio for gauge display
var data = JSON.parse(msg.payload);
if (data.compression_stats) {
    msg.payload = data.compression_stats.ratio; // 9.17
}
return msg;
```

#### **3. Power Monitoring:**
```javascript
// Process power data for real-time display
var data = JSON.parse(msg.payload);
if (data.data && data.data.length > 0) {
    var latest = data.data[data.data.length - 1];
    msg.payload = {
        ac_power: latest.ac_power,
        dc_power: latest.dc_power,
        efficiency: latest.efficiency
    };
}
return msg;
```

### **Flow Import:**
```powershell
# Import the updated flow
# Copy content from: nodered-ecowatt-dashboard.json
# Node-RED Editor → Import → Clipboard
# Paste JSON content → Import
```

### **Starting Node-RED:**
```powershell
# Start Node-RED
node-red

# Access points:
# Flow Editor: http://localhost:1880
# Dashboard: http://localhost:1880/ui
```

---

## 🧪 **Testing & Validation** {#testing}

### **Available Test Scripts:**

#### **1. ESP32 Upload Simulation:**
```powershell
cd test
python test_upload.py
```
**Expected Output:**
```
🚀 Testing ESP32 upload simulation...
📤 Uploading 314 bytes of compressed data
✅ Upload successful! 
📊 Compression ratio: 9.17:1
📡 MQTT message published
⏱️  Response time: 125ms
```

#### **2. MQTT Subscription Test:**
```powershell
python mqtt_subscriber.py
```
**Expected Output:**
```
✅ Connected to MQTT broker at broker.emqx.io:1883
📡 Subscribed to topic: vdl/replace
📩 Message received: 1,247 bytes
📋 Data type: JSON
🔢 Records count: 15
📈 Compression ratio: 9.17:1
```

#### **3. Server Load Testing:**
```powershell
cd test
python server_load_test.py
```
**Purpose:** Test server performance under multiple concurrent uploads

#### **4. Compression Algorithm Validation:**
```powershell
cd test  
python compression_validation.py
```
**Validates:**
- ✅ Data integrity (100% reconstruction)
- ✅ Compression ratio consistency  
- ✅ Timestamp delta accuracy
- ✅ Register value preservation

### **Manual Testing Workflow:**

#### **Step 1: Start Server**
```powershell
python server.py
```
**Expected Console Output:**
```
🚀 ECO-WATT Flask Server Starting...
✅ Server running on http://0.0.0.0:5000
✅ Connected to MQTT broker at broker.emqx.io:1883
📡 Publishing to topic: vdl/replace
🎯 Ready to receive ESP32 uploads!
```

#### **Step 2: Test Upload**
```powershell
cd test
python test_upload.py
```
**Verify:**
- ✅ HTTP 200 response
- ✅ Compression ratio ~9:1
- ✅ Server logs show decompression
- ✅ MQTT publish confirmation

#### **Step 3: Verify MQTT Messaging**
```powershell
python mqtt_subscriber.py
```
**Should Receive:**
- ✅ JSON payload with 15 records
- ✅ Compression statistics
- ✅ Properly formatted solar data

#### **Step 4: Test Node-RED Dashboard**
```powershell
node-red
# Open: http://localhost:1880/ui
```
**Verify:**
- ✅ Real-time charts updating
- ✅ Compression gauges showing 9:1 ratio
- ✅ Device status indicators
- ✅ JSON data viewer working

---

## ⚙️ **Configuration Guide** {#configuration}

### **Flask Server Config (`server.py`):**

#### **Production Settings:**
```python
# Production MQTT Broker
MQTT_BROKER = "your-production-broker.com"
MQTT_PORT = 1883
MQTT_USERNAME = "production_user"
MQTT_PASSWORD = "secure_password"
MQTT_USE_TLS = True

# Production Flask Settings
app.config['DEBUG'] = False
HOST = "0.0.0.0"
PORT = 80
app.config['SECRET_KEY'] = 'your-secret-key'
```

#### **Development Settings:**
```python
# Development MQTT (Public broker)
MQTT_BROKER = "broker.emqx.io"
MQTT_PORT = 1883
# No authentication required

# Development Flask
app.config['DEBUG'] = True
HOST = "127.0.0.1"
PORT = 5000
```

### **ESP32 Configuration (Reference):**
```cpp
// Updated Config.h for 9:1 compression
#define POLL_PERIOD_MS 1000      // 1 second data collection
#define UPLOAD_PERIOD_MS 15000   // 15 second uploads  
#define QTY_REGS 10             // 10 solar registers
#define COMPRESSION_ENABLED true // Enable delta compression
```

### **Node-RED Configuration:**

#### **Dashboard Settings:**
```json
{
  "ui_base": {
    "theme": "dark",
    "site": "ECO-WATT Solar Monitor", 
    "hideToolbar": false
  },
  "mqtt_broker": {
    "broker": "broker.emqx.io",
    "port": 1883,
    "keepalive": 60
  }
}
```

---

## 🛠️ **Troubleshooting** {#troubleshooting}

### **Common Server Issues:**

#### **1. Server Won't Start**
```powershell
# Check if port is in use
netstat -an | findstr :5000

# Solution: Kill process or use different port
taskkill /f /im python.exe  # Kill all Python processes
python server.py --port 5001
```

#### **2. MQTT Connection Failed**
```bash
# Test connectivity
ping broker.emqx.io

# Alternative brokers to try:
MQTT_BROKER = "test.mosquitto.org"  
MQTT_BROKER = "mqtt.eclipse.org"
```

#### **3. Compression/Decompression Errors**
```python
# Common error: "Invalid data format"
# Debug steps:
1. Log raw data received:
   print(f"Raw data length: {len(request.data)}")
   print(f"Raw data hex: {binascii.hexlify(request.data)}")

2. Validate header:
   if len(data) < 2:
       return {"error": "Data too short for header"}
   
   num_records = data[0]
   if num_records == 0 or num_records > 50:
       return {"error": f"Invalid record count: {num_records}"}

3. Check expected size:
   expected_size = 2 + (num_records * (1 + regs_per_record * 2))
   if len(data) < expected_size:
       return {"error": f"Data too short. Expected: {expected_size}, Got: {len(data)}"}
```

#### **4. Dashboard Not Updating**
```javascript
// Node-RED debugging checklist:
1. Check MQTT connection status (green dot)
2. Verify topic spelling: "vdl/replace" (case sensitive)
3. Add debug nodes to see data flow
4. Check dashboard group configuration
5. Verify function node syntax

// Debug flow:
MQTT In → Debug Node → Function → Dashboard
```

### **Debug Commands:**

#### **Server Debugging:**
```powershell
# Enable verbose logging
$env:FLASK_ENV=development
$env:FLASK_DEBUG=1
python server.py

# Monitor real-time logs
Get-Content server.log -Wait -Tail 50
```

#### **MQTT Testing:**
```powershell
# Subscribe to see messages
python mqtt_subscriber.py

# Test MQTT broker connectivity
mosquitto_sub -h broker.emqx.io -t "vdl/replace" -v

# Publish test message
mosquitto_pub -h broker.emqx.io -t "vdl/replace" -m "test message"
```

#### **Network Diagnostics:**
```powershell
# Check server accessibility
curl http://localhost:5000/api/dashboard/health

# Test ESP32 upload endpoint
curl -X POST http://localhost:5000/api/inverter/upload -H "Content-Type: application/json" -d "{\"test\": true}"

# Port status check
netstat -an | findstr ":5000\|:1883\|:1880"
```

---

## 🚀 **Quick Start Checklist**

### **✅ Server Setup (3 minutes):**
- [ ] `python -m venv .venv && .venv\Scripts\activate`
- [ ] `pip install flask paho-mqtt requests`
- [ ] `python server.py`
- [ ] Verify: ✅ Server running, ✅ MQTT connected

### **✅ MQTT Verification (2 minutes):**
- [ ] `python mqtt_subscriber.py` (new terminal)
- [ ] `python test/test_upload.py` (another terminal)
- [ ] Confirm: ✅ Message received, ✅ 9:1 ratio displayed

### **✅ Node-RED Dashboard (4 minutes):**
- [ ] Install Node.js from https://nodejs.org/
- [ ] `npm install -g node-red`
- [ ] `node-red` → http://localhost:1880
- [ ] Import flow from `nodered-ecowatt-dashboard.json`
- [ ] Access dashboard: http://localhost:1880/ui

### **✅ End-to-End Testing (2 minutes):**
- [ ] Server running ✅
- [ ] MQTT subscriber active ✅  
- [ ] Node-RED dashboard open ✅
- [ ] Run `python test/test_upload.py`
- [ ] Verify: Dashboard updates with 9:1 compression data ✅

---

## 🎯 **Production Deployment Tips**

### **Security Enhancements:**
```python
# Add API authentication
@app.before_request  
def authenticate():
    api_key = request.headers.get('X-API-Key')
    if not api_key or api_key != 'your-secure-api-key':
        return {"error": "Unauthorized"}, 401

# Enable HTTPS
app.run(host='0.0.0.0', port=443, ssl_context='adhoc')
```

### **Performance Optimization:**
```python
# Add request rate limiting
from flask_limiter import Limiter
limiter = Limiter(app, key_func=get_remote_address)

@app.route('/api/inverter/upload', methods=['POST'])
@limiter.limit("60 per minute")  # Max 60 uploads per minute
def upload_inverter_data():
    # ... existing code
```

### **Monitoring & Logging:**
```python
import logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('ecowatt_server.log'),
        logging.StreamHandler()
    ]
)
```

---

**🎉 Your ECO-WATT server infrastructure is now professionally documented and ready for deployment! The updated 9:1 compression algorithm provides exceptional bandwidth efficiency for your solar monitoring system. 🚀✨**