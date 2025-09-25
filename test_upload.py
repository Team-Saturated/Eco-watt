#!/usr/bin/env python3
"""
Test script to create realistic Record data matching Buffer.h/Transport.h structure,
compress it using Compression.cpp logic, and test the Flask server
"""

import requests
import binascii
import time

# Simulate the actual DecodedReg and Record structures from the C++ code
class DecodedReg:
    def __init__(self, addr, raw, value, unit):
        self.addr = addr    # uint16_t addr
        self.raw = raw      # uint16_t raw  
        self.value = value  # float value
        self.unit = unit    # String unit

class Record:
    def __init__(self, ts_ms, start, qty, regs=None, rawFrameHex=""):
        self.ts_ms = ts_ms          # uint64_t ts_ms
        self.start = start          # uint16_t start
        self.qty = qty              # uint16_t qty
        self.rawFrameHex = rawFrameHex  # String rawFrameHex
        self.regs = regs or []      # std::vector<DecodedReg> regs

def create_realistic_records():
    """Create realistic Record data matching what Acquisition.cpp would generate"""
    records = []
    base_time = int(time.time() * 1000)  # Current time in milliseconds
    
    # Create 15 records simulating solar inverter readings (like ESP32 would collect)
    for i in range(15):
        # Inverter register addresses from Acquisition.cpp:regScaleUnit()
        regs = [
            DecodedReg(0, 2200 + i*2, (2200 + i*2)/10.0, "V"),    # Vac1 / L1 Phase voltage (÷10)
            DecodedReg(1, 150 + i, (150 + i)/10.0, "A"),          # Iac1 / L1 Phase current (÷10)  
            DecodedReg(2, 5000 + i, (5000 + i)/100.0, "Hz"),      # Fac1 / L1 Phase frequency (÷100)
            DecodedReg(3, 3800 + i*3, (3800 + i*3)/10.0, "V"),   # Vpv1 / PV1 input voltage (÷10)
            DecodedReg(9, 4500 + i*10, 4500 + i*10, "W")         # Inverter current output power
        ]
        
        record = Record(
            ts_ms=base_time + i * 1000,  # 1 second apart
            start=0x0000,               # START_ADDR from Config.h
            qty=10,                     # QTY_REGS from Config.h
            regs=regs,
            rawFrameHex=f"110305{i:02d}A2B3C4D5E6F7{i:02d}FF"  # Simulated Modbus frame
        )
        records.append(record)
    
    return records

def compress_delta_python(records):
    """Python implementation of Compression.cpp compressDelta() logic"""
    if not records:
        return bytes()
    
    data = bytearray()
    prev_ts = records[0].ts_ms
    data.append(len(records))  # Number of records
    
    for r in records:
        # Delta timestamp compression
        delta = r.ts_ms - prev_ts
        prev_ts = r.ts_ms
        data.append(delta & 0xFF)  # Store delta (simplified to 1 byte)
        
        # Compress first register value (matches C++ logic)
        if r.regs:
            v = r.regs[0].value  # First register value
            vi = int(v * 100)    # Scale by 100 (matches C++)
            data.append((vi >> 8) & 0xFF)  # High byte
            data.append(vi & 0xFF)         # Low byte
        else:
            data.append(0)
            data.append(0)
    
    return bytes(data)

def decompress_and_compare(compressed_data, original_records):
    """Decompress data and compare with original for verification"""
    print("\n=== DECOMPRESSION VERIFICATION ===")
    
    if not compressed_data:
        print("❌ No compressed data to decompress")
        return
    
    data = compressed_data
    n = data[0]
    ts = original_records[0].ts_ms if original_records else 0
    idx = 1
    decompressed = []
    
    print(f"Decompressing {n} records...")
    
    for i in range(n):
        if idx + 2 >= len(data):
            break
        delta = data[idx]; idx += 1
        ts += delta
        vi = (data[idx] << 8) | data[idx+1]; idx += 2
        v = vi / 100.0
        decompressed.append({"ts_ms": ts, "value": v})
        
    print(f"✅ Successfully decompressed {len(decompressed)} records")
    
    # Compare first few records
    print("\nComparison (Original vs Decompressed):")
    for i in range(min(3, len(original_records), len(decompressed))):
        orig = original_records[i]
        decomp = decompressed[i]
        orig_val = orig.regs[0].value if orig.regs else 0
        print(f"  Record {i+1}: {orig_val:.2f}V -> {decomp['value']:.2f}V {'✅' if abs(orig_val - decomp['value']) < 0.01 else '❌'}")
    
    return decompressed

def create_test_compressed_data():
    """Create realistic compressed data using actual Record structure and compression logic"""
    # Create realistic records
    records = create_realistic_records()
    
    print("=== REALISTIC RECORD GENERATION ===")
    print(f"Created {len(records)} realistic inverter records")
    print("Sample records:")
    for i, r in enumerate(records[:3]):
        print(f"  Record {i+1}: ts={r.ts_ms}, regs={len(r.regs)}")
        if r.regs:
            print(f"    Vac1={r.regs[0].value:.1f}{r.regs[0].unit}, Iac1={r.regs[1].value:.1f}{r.regs[1].unit}")
    
    # Compress using actual algorithm
    compressed_data = compress_delta_python(records)
    
    print(f"\n=== COMPRESSION RESULTS ===")
    original_size = len(records) * 50  # Estimate: each Record ~50 bytes in memory
    compressed_size = len(compressed_data)
    ratio = original_size / compressed_size if compressed_size > 0 else 1
    
    print(f"Original size (est): {original_size} bytes")
    print(f"Compressed size: {compressed_size} bytes") 
    print(f"Compression ratio: {ratio:.2f}:1")
    print(f"Space saved: {((original_size - compressed_size) / original_size * 100):.1f}%")
    
    # Verify compression/decompression
    decompress_and_compare(compressed_data, records)
    
    return compressed_data

def test_flask_upload():
    """Test the Flask server upload endpoint with realistic compressed data"""
    print("=== FLASK SERVER UPLOAD TEST ===")
    
    compressed_data = create_test_compressed_data()
    hex_data = binascii.hexlify(compressed_data).decode()
    
    # Data matching what ESP32 Uploader.cpp sends
    json_data = {
        "data": hex_data,
        "size": len(compressed_data),
        "device_id": "ESP32_SOLAR_INV_001",
        "compression": "delta",
        "timestamp": int(time.time())
    }
    
    print(f"\nUploading {len(compressed_data)} bytes of compressed data...")
    print(f"Hex data preview: {hex_data[:40]}..." if len(hex_data) > 40 else f"Hex data: {hex_data}")
    
    try:
        response = requests.post('http://localhost:5000/api/inverter/upload', 
                               json=json_data, 
                               timeout=10)
        
        print(f"\n=== SERVER RESPONSE ===")
        print(f"Status Code: {response.status_code}")
        print(f"Response Headers: {dict(response.headers)}")
        
        if response.status_code == 200:
            try:
                json_response = response.json()
                print(f"✅ Upload successful!")
                print(f"Server processed: {json_response.get('records_processed', 'unknown')} records")
                print(f"Server message: {json_response.get('message', 'No message')}")
                
                # Show feedback info
                feedback = json_response.get('feedback', {})
                if feedback:
                    print(f"Server feedback:")
                    print(f"  Next upload in: {feedback.get('next_upload_interval', 'default')} seconds")
                    print(f"  Quality: {feedback.get('data_quality', 'good')}")
                    print(f"  Commands: {feedback.get('commands', [])}")
                    
            except:
                print(f"Response (text): {response.text}")
        else:
            print("❌ Upload failed!")
            print(f"Error: {response.text}")
            
    except requests.exceptions.ConnectionError:
        print("❌ Connection failed! Make sure Flask server is running:")
        print("   python server.py")
    except requests.exceptions.RequestException as e:
        print(f"❌ Request failed: {e}")

def test_dashboard():
    """Test the dashboard endpoint"""
    print("\n=== DASHBOARD TEST ===")
    try:
        response = requests.get('http://localhost:5000/', timeout=5)
        if response.status_code == 200:
            print("✅ Dashboard accessible at http://localhost:5000/")
        else:
            print(f"❌ Dashboard error: {response.status_code}")
    except:
        print("❌ Dashboard not accessible")

def test_esp32_style_upload(server_url="http://127.0.0.1:5000/api/inverter/upload"):
    """Send test data to server and show response"""
    
    # Create realistic test data
    compressed_data = create_test_compressed_data()
    payload_hex = binascii.hexlify(compressed_data).decode('utf-8').upper()
    
    print("=== TESTING ESP32 COMPRESSION UPLOAD ===")
    print(f"Compressed data (hex): {payload_hex}")
    print(f"Compressed size: {len(compressed_data)} bytes")
    print(f"Sending to: {server_url}")
    print()
    
    try:
        # Send POST request (same as ESP32 would do)
        response = requests.post(
            server_url,
            data=payload_hex,
            headers={'Content-Type': 'application/octet-stream'}
        )
        
        print(f"Response Status: {response.status_code}")
        print(f"Response Body: {response.text}")
        
        if response.status_code == 200:
            print("\n✅ SUCCESS: Server received and processed data correctly!")
        else:
            print(f"\n❌ ERROR: Server returned {response.status_code}")
            
    except Exception as e:
        print(f"❌ FAILED to connect: {e}")

if __name__ == "__main__":
    print("🔋 EcoWatt Solar Inverter Data Compression & Upload Test")
    print("=" * 60)
    
    # Test Flask server upload (JSON format)
    test_flask_upload()
    
    # Test dashboard
    test_dashboard()
    
    # Test ESP32-style upload (binary format) - optional
    print("\n" + "-" * 40)
    test_esp32_style_upload()
    
    print("\n" + "=" * 60)
    print("💡 To view the dashboard, open: http://localhost:5000/")
    print("📊 Real-time data should appear after successful uploads")