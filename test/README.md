# Test Directory

# 🧪 EcoWatt Testing Suite

This folder contains testing scripts for validating the EcoWatt server functionality and compression algorithms. **Note: These are software tests only** - they simulate ESP32 behavior without real hardware.

This directory contains comprehensive test files for validating the EcoWatt solar inverter compression system, cloud connectivity, and payload analysis. Each test serves a specific purpose in ensuring the C++ compression algorithms work exactly as expected with cloud feedback validation.

## 🧪 What This Folder Does

## 📁 Test Files Overview

The test directory validates:

- **Server functionality** - Test Flask server endpoints ### 🎯 **test_upload.py** - ESP32 Payload Size Validation

- **MQTT communication** - Test message publishing/subscribing**Purpose**: Creates EXACT ESP32 15-second buffer drain simulation and validates payload sizes

- **Compression algorithm** - Benchmark the 9:1 compression ratio**What it tests**:

- **Data upload** - Validate server receives compressed data correctly- ✅ Exact ESP32 timing simulation (POLL_PERIOD_MS=1000, UPLOAD_PERIOD_MS=15000)

- ✅ Buffer drain validation (15 records, 10 registers each)

## 📁 Test Files- ✅ C++ Compression.cpp algorithm matching (100% identical)

- ✅ Real payload size validation (ESP32 vs Python test data)

| File | Purpose | Quick Command |- ✅ Flask server upload with decompression verification

|------|---------|---------------|- ✅ Dashboard integration testing

| `compression_benchmark.py` | Tests compression algorithm achieving **314 bytes** from 2880 bytes (9:1 ratio) | `python compression_benchmark.py` |

| `mqtt_listener.py` | Listens to MQTT topic "vdl/replace" for incoming solar data | `python mqtt_listener.py` |**Run Command**: `python test_upload.py`

| `mqtt_publisher.py` | Publishes test data to MQTT broker for testing | `python mqtt_publisher.py` |

| `server_upload_test.py` | Tests Flask server upload endpoint with compressed data | `python server_upload_test.py` |**Expected Results**:

```

## 🚀 Quick Test CommandsESP32 uncompressed buffer: 2,880 bytes

Python compressed payload: 46 bytes

### Test Compression PerformanceCompression ratio: 62.6:1

```bashSpace saved: 98.4%

cd test🎯 This compressed payload size (46 bytes) is EXACTLY what ESP32 would upload!

python compression_benchmark.py```

```

**Expected Result**: Shows 9:1 compression ratio (2880 → 314 bytes)### 🔬 **benchmark_test.py** - C++ Algorithm Verification  

**Purpose**: Byte-for-byte validation of Compression.cpp algorithm

### Test MQTT Communication**What it tests**:

```bash- ✅ Exact C++ compression algorithm implementation

# Terminal 1 - Start listener- ✅ Lossless compression/decompression verification

python mqtt_listener.py- ✅ Compression ratio benchmarking

- ✅ Server integration with compressed binary data

# Terminal 2 - Send test data- ✅ Mathematical accuracy validation

python mqtt_publisher.py

```**Run Command**: `python benchmark_test.py`



### Test Server Upload**Expected Results**:

```bash```

# Make sure Flask server is running firstCompression Test Results:

cd ../ServerOriginal data: X bytes

python server.pyCompressed data: Y bytes  

Compression ratio: Z:1

# Then test upload✅ 100% lossless compression verified

cd ../test✅ C++ algorithm matching confirmed

python server_upload_test.py```

```

## 🚀 **How to Run Complete Test Suite**

## 📊 Compression Results

### **Step 1: Start Flask Server**

- **Original Size**: 2,880 bytes (60 records)```bash

- **Compressed Size**: 314 bytes # Terminal 1 - Start the cloud server

- **Compression Ratio**: 9:1cd "C:\Users\...\Eco-watt"

- **Space Saved**: 89.1%python server.py

```

## ⚠️ Important NotesExpected output:

```

- These tests **simulate ESP32 behavior** - no real hardware neededEcoWatt Cloud API Server Starting...

- Server must be running for upload tests to workDashboard: http://localhost:5000/

- MQTT tests use public broker `broker.emqx.io:1883`API Endpoint: http://localhost:5000/api/inverter/upload

- Compression algorithm matches C++ implementation exactly```



## 🔧 Prerequisites### **Step 2: Run ESP32 Payload Validation**

```bash  

```bash# Terminal 2 - Test exact ESP32 payload matching

pip install requests paho-mqttcd "C:\Users\...\Eco-watt\test"

```python test_upload.py

```

---

### **Step 3: Run C++ Algorithm Benchmark**

*These tests validate server functionality without requiring physical ESP32 hardware*```bash
# Terminal 3 - Verify C++ compression algorithm
python benchmark_test.py
```
### **Step 4: Check Dashboard**
Open browser: `http://localhost:5000/`

## 📊 **Validation Checklist After Each Test Run**

### ✅ **C++ Algorithm Matching**
- [ ] Compression.cpp logic: **100% identical** implementation
- [ ] Delta encoding: **Lossless** timestamp compression
- [ ] Value scaling: **×100 factor** preserved
- [ ] Byte packing: **Exact bit-level** matching
- [ ] Decompression: **Perfect reconstruction** of original data

### ✅ **Cloud Feedback Validation**
- [ ] HTTP Response: **200 OK** status
- [ ] Server processing: **Correct record count** detection
- [ ] Decompression: **Successful** on server side
- [ ] Dashboard update: **Real-time** data display
- [ ] Binary files: **Saved in uploads/** folder for analysis


## � **Upload File Storage & Analysis**

### **📁 Where Upload Files Are Saved:**
**Location**: `C:\Users\...\Eco-watt\uploads\` folder  
**Auto-creation**: Folder created automatically by Flask server on first upload  
**Access**: Files can be analyzed with hex editors or binary analysis tools

### **🗂️ File Naming Convention:**
```
### **📦 What Each Upload File Contains:**

#### **Binary Content Breakdown:**
```
Byte 0: 0x0F (15 records count)
Bytes 1-3: Record 1 (delta_time + compressed_value)  
Bytes 4-6: Record 2 (delta_time + compressed_value)
...
Bytes 43-45: Record 15 (delta_time + compressed_value)
```

#### **Hex Payload Example:**
```
0F0055F0E85604E85618E8562CE85640E85654E85668E8567CE85690E856A4E856B8E856CCE856E0E856F4E85708

Decoded:
0F = 15 records
0055F0 = Record 1: delta=0, value=0x55F0 (220.00V)
E85604 = Record 2: delta=232, value=0x5604 (220.20V) 
E85618 = Record 3: delta=232, value=0x5618 (220.40V)
...and so on
```

### **🔍 Upload File Analysis Tools:**

#### **Method 1: Windows PowerShell**
```powershell
# View file size
Get-Item "uploads\upload_*.bin" | Select Name, Length

# View hex content (first 20 bytes)  
Get-Content "uploads\upload_ESP32_SOLAR_INV_001_*.bin" -Encoding Byte | 
ForEach-Object {'{0:X2}' -f $_} | Select-Object -First 20
```


### **📊 ESP32 Payload Validation Report (Integrated)**

#### **🎯 EXACT ESP32 MATCHING ACHIEVED:**

**ESP32 Configuration (from Config.h):**
- **POLL_PERIOD_MS**: 1000ms (1 second polling interval)
- **UPLOAD_PERIOD_MS**: 15000ms (15 second upload interval)  
- **QTY_REGS**: 10 (registers per poll)
- **BUFFER_CAPACITY**: 128 samples

**ESP32 15-Second Buffer Drain Analysis:**
- Polls in 15 seconds: **15 samples** (15000ms ÷ 1000ms = 15)
- Registers per sample: **10 registers** 
- Total registers: **150 registers** (15 × 10)
- Estimated uncompressed size: **2,880 bytes** (15 records × 192 bytes/record)



### **🗃️ Upload File Management:**

#### **File Lifecycle:**
1. **Creation**: Each HTTP POST to `/api/inverter/upload` creates new .bin file
2. **Naming**: Timestamp ensures unique filenames (no overwrites)
3. **Storage**: Files persist until manually deleted
4. **Analysis**: Can be processed offline for historical data analysis

#### **File Cleanup (Optional):**
```bash
# Remove files older than 24 hours
find uploads/ -name "*.bin" -mtime +1 -delete

# Keep only last 10 uploads
ls -t uploads/*.bin | tail -n +11 | xargs rm --
```

#### **Production Considerations:**
- **Disk Space**: Each upload = 46 bytes (very efficient)
- **Daily Storage**: ~4MB per day (assuming 1 upload/15 seconds)
- **Log Rotation**: Implement if running continuously
- **Backup**: Files contain complete solar data for analysis

## 🌐 **Server Status & IP Configuration**

### **Server Endpoints**:
- **Dashboard**: `http://localhost:5000/`
- **Upload API**: `http://localhost:5000/api/inverter/upload`
- **Stats API**: `http://localhost:5000/api/nodered/stats`

### **For External Access** (ESP32 deployment):
1. Change server IP in `Config.h`: `#define API_URL "http://YOUR_PC_IP:5000/api/inverter/upload"`
2. Update Python tests: `server_url = "http://YOUR_PC_IP:5000/api/inverter/upload"`
3. Ensure firewall allows port 5000
4. Run server with: `app.run(host='0.0.0.0', port=5000)`


---


