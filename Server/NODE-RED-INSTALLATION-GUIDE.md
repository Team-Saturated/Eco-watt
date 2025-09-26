# 🚀 Node-RED EcoWatt Dashboard Installation & Usage Guide

## 📊 **Understanding the Data Flow**

### **Data Being Sent to Flask Server:**

#### **🔹 Upload Endpoint** (`/api/inverter/upload`)
**Data Format**: JSON or Binary compressed payload
```json
{
  "data": "0F0055F0E85604E85618E8562CE85640E85654E85668E8567CE85690E856A4E856B8E856CCE856E0E856F4E85708",
  "device_id": "ESP32_SOLAR_INV_001",
  "size": 46
}
```

**What this represents**:
- **46 bytes** of compressed solar inverter data
- **15 records** (0x0F) collected over 15 seconds
- **10 registers per record** (voltage, current, power, frequency, etc.)
- **Delta-compressed timestamps** and **scaled values** (×100)

#### **🔹 Node-RED API Endpoints** (Auto-created by Flask server)
1. **`/api/nodered/stats`** - Compression statistics
2. **`/api/nodered/solar-data`** - Decompressed solar inverter readings  
3. **`/api/nodered/compression-history`** - Historical compression analytics

## 🛠️ **Installation Steps**

### **Step 1: Install Node.js**
```bash
# Download and install Node.js from: https://nodejs.org/
# Verify installation:
node --version
npm --version
```

### **Step 2: Install Node-RED**
```bash
# Install Node-RED globally
npm install -g node-red

# Verify installation
node-red --version
```

### **Step 3: Install Required Node-RED Modules**
```bash
# Navigate to Node-RED user directory
cd %USERPROFILE%\.node-red

# Install dashboard module
npm install node-red-dashboard

# Install additional useful modules
npm install node-red-contrib-ui-led
npm install node-red-node-smooth
```

### **Step 4: Start Node-RED**
```bash
# Start Node-RED (will create default config)
node-red

# Expected output:
# Welcome to Node-RED
# Server now running at http://127.0.0.1:1880/
```

### **Step 5: Import EcoWatt Dashboard Flow**

#### **Method 1: Import via Web Interface**
1. Open Node-RED editor: `http://localhost:1880/`
2. Click **hamburger menu (☰)** → **Import**
3. Copy the entire contents of `nodered-ecowatt-dashboard.json`
4. Paste into import dialog
5. Click **Import**

#### **Method 2: Import via File**
```bash
# Copy the JSON file to Node-RED directory
copy "nodered-ecowatt-dashboard.json" "%USERPROFILE%\.node-red\flows\"

# Restart Node-RED to load the flow
```

## 🎯 **Testing & Usage Commands**

### **Terminal 1: Start Flask Server**
```bash
cd "C:\Users\Yasiru Alahakoon\Desktop\Image Processing And Machine Vision\EN3160 Assignment 3 on Neural Networks\Eco-watt"
python server.py
```
**Expected Output**:
```
EcoWatt Cloud API Server Starting...
Dashboard: http://localhost:5000/
API Endpoint: http://localhost:5000/api/inverter/upload
```

### **Terminal 2: Start Node-RED**
```bash
node-red
```
**Expected Output**:
```
Welcome to Node-RED
Server now running at http://127.0.0.1:1880/
Dashboard: http://127.0.0.1:1880/ui/
```

### **Terminal 3: Run ESP32 Simulation Tests**
```bash
cd "C:\Users\Yasiru Alahakoon\Desktop\Image Processing And Machine Vision\EN3160 Assignment 3 on Neural Networks\Eco-watt\test"

# Run ESP32 payload validation test
python test_upload.py

# Run C++ compression benchmark  
python benchmark_test.py
```

### **Terminal 4: Monitor Node-RED API Calls**
```bash
# Test Node-RED API endpoints directly
curl http://localhost:5000/api/nodered/stats
curl http://localhost:5000/api/nodered/solar-data  
curl http://localhost:5000/api/nodered/compression-history
```

## 🌐 **Dashboard Access URLs**

| Service | URL | Purpose |
|---------|-----|---------|
| **Flask Dashboard** | `http://localhost:5000/` | Server-side compression stats |
| **Node-RED Editor** | `http://localhost:1880/` | Flow editor and debugging |
| **Node-RED Dashboard** | `http://localhost:1880/ui/` | **🎯 MAIN DASHBOARD** |
| **Node-RED Admin** | `http://localhost:1880/admin/` | Node-RED administration |

## 📊 **Dashboard Features**

### **🔹 Compression Statistics Panel**
- **Device ID**: ESP32 solar inverter identifier
- **Total Uploads**: Count of successful data uploads
- **Records**: Number of time-series records (should be 15)
- **Original Size**: Uncompressed data size (~2,880 bytes)
- **Compressed Size**: Compressed payload size (46 bytes)
- **Compression Ratio**: Efficiency ratio (should be ~62:1)
- **Space Saved**: Bandwidth reduction percentage (~98%)

### **🔹 Solar Voltage Chart**
- **Real-time line chart** showing voltage readings over time
- **X-axis**: Time (HH:mm:ss format)
- **Y-axis**: Voltage (200-250V range)
- **Data source**: Decompressed ESP32 register readings

### **🔹 Current Voltage Gauge**
- **Real-time gauge** showing latest voltage reading
- **Green zone**: 230-250V (optimal)
- **Yellow zone**: 210-230V (acceptable)
- **Red zone**: 200-210V (low voltage warning)

### **🔹 Compression History**
- **Line chart**: Compression ratio trends over time
- **Bar chart**: Original vs compressed bytes comparison
- **Historical analysis**: Last 10 upload cycles

## 🔧 **Terminal Commands for Monitoring**

### **Check Flask Server Status**
```bash
# Test server is running
curl http://localhost:5000/

# Check API endpoints
curl http://localhost:5000/api/nodered/stats | python -m json.tool
curl http://localhost:5000/api/nodered/solar-data | python -m json.tool
```

### **Monitor Upload Files**
```bash
cd "C:\Users\Yasiru Alahakoon\Desktop\Image Processing And Machine Vision\EN3160 Assignment 3 on Neural Networks\Eco-watt"

# List upload files
dir uploads\*.bin

# Check file sizes (should be 46 bytes each)
powershell "Get-ChildItem uploads\*.bin | Select-Object Name, Length"

# View hex content of latest upload
powershell "Get-Content uploads\upload_*.bin -Encoding Byte | ForEach-Object {'{0:X2}' -f $_} | Out-String"
```

### **Node-RED Debugging**
```bash
# Check Node-RED logs
node-red --verbose

# Test dashboard connectivity
curl http://localhost:1880/ui/

# Check Node-RED flows
curl http://localhost:1880/flows
```

## 📈 **Expected Results & Validation**

### **✅ Successful Dashboard Operation**

#### **After Running `python test_upload.py`**:
1. **Flask Server Terminal**: Shows compression stats
```
[UPLOAD] Compressed batch size: 46 bytes
Decompressed to 15 records
Compression ratio: 62.6:1
```

2. **Node-RED Dashboard** (`http://localhost:1880/ui/`):
   - **Compression Stats**: Updates with 15 records, 46 bytes compressed
   - **Voltage Chart**: Shows 15 data points over ~15 seconds
   - **Voltage Gauge**: Displays latest reading (~220V)
   - **History Charts**: Show compression trends

#### **Data Flow Validation**:
```
ESP32 Test → Flask Server → Node-RED APIs → Dashboard Widgets
    ↓              ↓              ↓              ↓
46 bytes    →  Decompress  →  JSON APIs  →  Real-time GUI
15 records  →  Statistics →  HTTP calls →  Charts/Gauges
```

### **🔍 Troubleshooting Common Issues**

#### **Problem**: Dashboard shows "No data received"
```bash
# Check Flask server is running
curl http://localhost:5000/api/nodered/stats

# Run test upload to generate data
python test\test_upload.py

# Verify Node-RED can reach Flask server
curl http://localhost:1880/flows | findstr "localhost:5000"
```

#### **Problem**: Node-RED dashboard not accessible
```bash
# Check if Node-RED is running
netstat -an | findstr :1880

# Restart Node-RED with verbose logging
node-red --verbose

# Check dashboard module installation
npm list node-red-dashboard
```

#### **Problem**: Flask server connection refused
```bash
# Check if Flask server is running
netstat -an | findstr :5000

# Start Flask server if not running
python server.py

# Test connectivity
telnet localhost 5000
```

## 🎯 **Complete Testing Workflow**

### **Run this sequence to see full system operation**:

```bash
# Terminal 1: Start Flask Server
python server.py

# Terminal 2: Start Node-RED (new terminal)  
node-red

# Terminal 3: Generate test data (new terminal)
cd test
python test_upload.py
python benchmark_test.py

# Browser 1: Open Node-RED Dashboard
start http://localhost:1880/ui/

# Browser 2: Open Flask Dashboard  
start http://localhost:5000/

# Terminal 4: Monitor uploads (new terminal)
dir uploads\*.bin
powershell "Get-ChildItem uploads\*.bin | Select-Object Name, Length"
```

### **Expected Timeline**:
- **0-10 seconds**: Servers starting up
- **10-30 seconds**: Import Node-RED flow, test data generation
- **30+ seconds**: Real-time dashboard updates every 15 seconds

### **Success Indicators**:
- ✅ **Flask Dashboard**: Shows 46-byte uploads, 62:1 compression
- ✅ **Node-RED Dashboard**: Real-time voltage charts and compression stats  
- ✅ **Upload Files**: 46-byte .bin files created in uploads/ folder
- ✅ **Terminal Logs**: Successful HTTP 200 responses and decompression

**🎉 Complete EcoWatt solar inverter monitoring system with Node-RED visualization!**