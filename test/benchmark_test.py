#!/usr/bin/env python3
"""
EXACT C++ Compression Benchmark Test
Simulates exactly what Compression.cpp does - creates 1 compressed packet
for benchmark analysis matching the C++ algorithm byte-for-byte
"""

import time
import binascii
import requests
from typing import List

# Exact C++ structure simulation
class DecodedReg:
    def __init__(self, addr=0, raw=0, value=0.0, unit=""):
        self.addr = addr    # uint16_t addr
        self.raw = raw      # uint16_t raw
        self.value = value  # float value
        self.unit = unit    # String unit

class Record:
    def __init__(self):
        self.ts_ms = 0              # uint64_t ts_ms
        self.start = 0              # uint16_t start  
        self.qty = 0                # uint16_t qty
        self.rawFrameHex = ""       # String rawFrameHex
        self.regs = []              # std::vector<DecodedReg> regs

def create_exact_cpp_records(num_records=10) -> List[Record]:
    """Create records exactly as C++ would - for benchmark testing"""
    records = []
    base_ts = int(time.time() * 1000)  # Current timestamp in ms
    
    print(f"📊 Creating {num_records} Records (exact C++ simulation)")
    
    for i in range(num_records):
        record = Record()
        record.ts_ms = base_ts + (i * 1000)  # 1 second intervals
        record.start = 0x0000  # START_ADDR from Config.h
        record.qty = 10        # QTY_REGS from Config.h
        
        # Create exactly 1 register (first register only - matches C++ algo)
        reg = DecodedReg()
        reg.addr = 0           # First register address
        reg.raw = 2200 + i     # Raw Modbus value (2200, 2201, 2202...)
        reg.value = (1200 + i) / 10.0  # Scaled value (220.0V, 220.1V, 220.2V...)
        reg.unit = "V"         # Voltage unit
        
        record.regs = [reg]    # Only first register (matches C++ algorithm)
        record.rawFrameHex = f"110304{(2200+i):04X}FFFF"  # Simulated Modbus frame
        
        records.append(record)
    
    return records

def compress_delta_exact_cpp(records: List[Record]) -> bytes:
    """
    EXACT implementation of Compression::compressDelta from C++
    Matches src/Compression.cpp line-by-line
    """
    out = bytearray()
    
    if not records:
        return bytes(out)
    
    # First record timestamp as base
    prev_ts = records[0].ts_ms
    
    # Store number of records (uint8_t)
    out.append(len(records))
    
    print(f"🔄 Compressing {len(records)} records using exact C++ algorithm...")
    
    for i, r in enumerate(records):
        # Calculate delta timestamp
        delta = r.ts_ms - prev_ts
        prev_ts = r.ts_ms
        
        # Store delta (1 byte for demo - matches C++ comment)
        out.append(delta & 0xFF)
        
        # Store first register value (matches C++ exactly)
        if r.regs:
            v = r.regs[0].value        # float v = r.regs[0].value
            vi = int(v * 100)          # uint16_t vi = (uint16_t)(v * 100)
            out.append((vi >> 8) & 0xFF)  # High byte
            out.append(vi & 0xFF)         # Low byte
            
            print(f"  Record {i+1}: delta={delta}ms, value={v:.2f}V -> {vi} (0x{vi:04X})")
        else:
            # No registers - store zeros (matches C++)
            out.append(0)
            out.append(0)
    
    return bytes(out)

def decompress_delta_exact_cpp(data: bytes) -> List[Record]:
    """
    EXACT implementation of Compression::decompressDelta from C++
    Matches src/Compression.cpp decompression algorithm
    """
    out = []
    
    if not data:
        return out
    
    n = data[0]           # size_t n = data[0]
    ts = 0                # uint64_t ts = 0  
    idx = 1               # size_t idx = 1
    
    print(f"🔄 Decompressing {n} records using exact C++ algorithm...")
    
    for i in range(n):
        if idx + 2 >= len(data):
            break
            
        # Read delta and reconstruct timestamp
        delta = data[idx]     # uint8_t delta = data[idx++]
        idx += 1
        ts += delta           # ts += delta
        
        # Read compressed value  
        vi = (data[idx] << 8) | data[idx + 1]  # uint16_t vi = (data[idx++] << 8) | data[idx++]
        idx += 2
        v = vi / 100.0        # float v = vi / 100.0f
        
        # Create Record (matches C++ exactly)
        r = Record()
        r.ts_ms = ts
        
        reg = DecodedReg()
        reg.value = v
        r.regs = [reg]
        
        out.append(r)
        print(f"  Record {i+1}: ts={ts}ms, value={v:.2f}V (from 0x{vi:04X})")
    
    return out

def benchmark_exact_cpp_compression():
    """
    Benchmark test matching exactly what C++ Compression.cpp does
    Creates 1 compressed packet for analysis
    """
    print("🔋 EXACT C++ Compression Benchmark Test")
    print("=" * 60)
    print("Matching Compression.cpp algorithm byte-for-byte")
    print()
    
    # Create test records (same as C++ would)
    records = create_exact_cpp_records(10)
    
    # Show original data structure
    print(f"\n📋 Original Records Structure:")
    print(f"  Total Records: {len(records)}")
    print(f"  Memory per Record: ~{8+2+2+20+16} bytes (ts+start+qty+hex+1reg)")
    original_size = len(records) * 48  # Realistic C++ struct size
    print(f"  Estimated Total: {original_size} bytes")
    
    # Show sample data
    print(f"\n📊 Sample Record Data:")
    for i, r in enumerate(records[:3]):
        print(f"  Record {i+1}: ts={r.ts_ms}, value={r.regs[0].value:.2f}V (raw={r.regs[0].raw})")
    
    # EXACT C++ compression
    start_time = time.time()
    compressed_data = compress_delta_exact_cpp(records)
    compress_time = (time.time() - start_time) * 1000  # ms
    
    # Show compression results
    print(f"\n📦 Compression Results (EXACT C++ Algorithm):")
    print(f"  Original Size: {original_size} bytes")
    print(f"  Compressed Size: {len(compressed_data)} bytes")
    print(f"  Compression Ratio: {original_size/len(compressed_data):.2f}:1")
    print(f"  Space Saved: {((original_size-len(compressed_data))/original_size*100):.1f}%")
    print(f"  Compression Time: {compress_time:.2f}ms")
    
    # Show hex packet (exactly what ESP32 would send)
    hex_packet = binascii.hexlify(compressed_data).decode().upper()
    print(f"\n📡 Compressed Packet (Hex - what ESP32 sends):")
    print(f"  {hex_packet}")
    print(f"  Length: {len(hex_packet)} hex chars ({len(compressed_data)} bytes)")
    
    # Packet breakdown
    print(f"\n🔍 Packet Structure Analysis:")
    print(f"  Byte 0: 0x{compressed_data[0]:02X} = {compressed_data[0]} records")
    for i in range(min(3, compressed_data[0])):
        base = 1 + i * 3
        if base + 2 < len(compressed_data):
            delta = compressed_data[base]
            value = (compressed_data[base+1] << 8) | compressed_data[base+2]
            print(f"  Bytes {base}-{base+2}: delta={delta}ms, value={value/100.0:.2f}V (0x{value:04X})")
    
    # EXACT C++ decompression verification
    start_time = time.time()
    decompressed = decompress_delta_exact_cpp(compressed_data)
    decompress_time = (time.time() - start_time) * 1000  # ms
    
    print(f"\n✅ Decompression Verification:")
    print(f"  Decompressed Records: {len(decompressed)}")
    print(f"  Decompression Time: {decompress_time:.2f}ms")
    
    # Verify accuracy
    all_correct = True
    print(f"\n🎯 Accuracy Check (Original vs Decompressed):")
    for i in range(min(len(records), len(decompressed))):
        orig_val = records[i].regs[0].value
        decomp_val = decompressed[i].regs[0].value
        is_correct = abs(orig_val - decomp_val) < 0.01
        status = "✅" if is_correct else "❌"
        print(f"  Record {i+1}: {orig_val:.2f}V -> {decomp_val:.2f}V {status}")
        if not is_correct:
            all_correct = False
    
    print(f"\n🏆 Benchmark Summary:")
    print(f"  Algorithm: Delta Encoding (exact C++ match)")
    print(f"  Compression: {original_size} -> {len(compressed_data)} bytes ({original_size/len(compressed_data):.1f}:1)")
    print(f"  Performance: {compress_time:.2f}ms compress, {decompress_time:.2f}ms decompress")
    print(f"  Accuracy: {'100% Perfect' if all_correct else 'ERRORS DETECTED'}")
    print(f"  Packet Ready: {len(compressed_data)} bytes for ESP32 upload")
    
    return compressed_data, hex_packet

def upload_to_flask_server(compressed_packet, hex_data):
    """Upload the benchmark data to Flask server to see dashboard results"""
    import requests
    
    print(f"\n" + "="*60)
    print(f"📡 UPLOADING BENCHMARK DATA TO FLASK SERVER")
    print(f"   Packet Size: {len(compressed_packet)} bytes")
    print(f"   Hex Data: {hex_data}")
    
    # Upload to Flask server
    json_data = {
        "data": hex_data,
        "size": len(compressed_packet),
        "device_id": "ESP32_BENCHMARK_EXACT_CPP",
        "compression": "delta_exact_cpp",
        "timestamp": int(time.time())
    }
    
    try:
        response = requests.post('http://localhost:5000/api/inverter/upload', 
                               json=json_data, 
                               timeout=10)
        
        if response.status_code == 200:
            json_response = response.json()
            print(f"✅ BENCHMARK DATA UPLOADED SUCCESSFULLY!")
            print(f"   Server processed: {json_response.get('num_records', 0)} records")
            print(f"   Server compression ratio: {json_response.get('compression_ratio', 0):.1f}:1")
            print(f"   Server space saved: {json_response.get('space_saved_percent', 0):.1f}%")
            print(f"\n🌐 VIEW BENCHMARK RESULTS ON DASHBOARD:")
            print(f"   URL: http://localhost:5000/")
            print(f"   Dashboard shows exact C++ benchmark data!")
            return True
        else:
            print(f"❌ Upload failed: {response.status_code}")
            print(f"   Response: {response.text}")
            return False
            
    except requests.exceptions.ConnectionError:
        print("❌ Connection failed!")
        print("   Make sure Flask server is running: python server.py")
        print("   Server should be at: http://localhost:5000/")
        return False
    except Exception as e:
        print(f"❌ Upload error: {e}")
        return False

if __name__ == "__main__":
    # Run exact C++ compression benchmark
    compressed_packet, hex_data = benchmark_exact_cpp_compression()
    
    # Ask user if they want to upload to Flask server
    print(f"\n" + "="*60)
    upload_choice = input("Upload benchmark data to Flask server? (y/n): ").lower().strip()
    
    if upload_choice in ['y', 'yes']:
        success = upload_to_flask_server(compressed_packet, hex_data)
        if success:
            print(f"\n🎯 BENCHMARK COMPLETE WITH FLASK UPLOAD!")
            print(f"   Check dashboard at http://localhost:5000/ to see results")
        else:
            print(f"\n⚠️ BENCHMARK COMPLETE (local only)")
            print(f"   Start Flask server first: python server.py")
    else:
        print(f"\n🏁 BENCHMARK COMPLETE (local testing only)")
        print(f"   Packet ready for manual upload: {hex_data}")