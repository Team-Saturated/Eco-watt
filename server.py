from flask import Flask, request, jsonify
import os, binascii, time

app = Flask(__name__)
UPLOAD_FOLDER = 'uploads'
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

# --- Global state for demo dashboard ---
stats = {
    "uploads": 0,
    "last_num_records": 0,
    "last_original_bytes": 0,
    "last_compressed_bytes": 0,
    "last_ratio": 0.0,
    "last_space_saved": 0.0,
    "last_raw_payload": "",
    "last_decompressed_data": []
}

def decompress_delta(data: bytes):
    """Decompress delta-encoded data matching Compression.cpp algorithm"""
    if not data:
        return []
    
    n = data[0]  # Number of records
    ts = int(time.time() * 1000)  # Base timestamp in milliseconds
    idx = 1
    out = []
    
    print(f"🔄 Decompressing {n} records from {len(data)} bytes...")
    
    for i in range(n):
        if idx + 2 >= len(data):
            print(f"⚠️ Data truncated at record {i}")
            break
            
        # Read delta and reconstruct timestamp
        delta = data[idx]; idx += 1
        ts += delta * 1000  # Convert delta to milliseconds
        
        # Read compressed value (2 bytes, scaled by 100)
        vi = (data[idx] << 8) | data[idx+1]; idx += 2
        voltage = vi / 100.0  # Scale back to real voltage
        
        record = {
            "ts_ms": ts,
            "value": voltage,
            "voltage": voltage,
            "unit": "V",
            "register": "Vac1",
            "device_id": "ESP32_SOLAR"
        }
        out.append(record)
        
    print(f"✅ Successfully decompressed {len(out)} records")
    return out

@app.route('/')
def index():
    """Enhanced dashboard view in browser"""
    decompressed_preview = ""
    if stats['last_decompressed_data']:
        decompressed_preview = "<br>".join([
            f"Record {i+1}: ts={r['ts_ms']}ms, value={r['value']:.2f}V" 
            for i, r in enumerate(stats['last_decompressed_data'][:5])
        ])
        if len(stats['last_decompressed_data']) > 5:
            decompressed_preview += f"<br>... and {len(stats['last_decompressed_data'])-5} more records"
    
    return f"""
    <h1>EcoWatt Cloud API Dashboard</h1>
    <div style="font-family: monospace; background: #f0f0f0; padding: 10px;">
    <h3>📊 Compression Statistics</h3>
    <p><b>Total uploads:</b> {stats['uploads']}</p>
    <p><b>Last records received:</b> {stats['last_num_records']}</p>
    <p><b>Original payload size (est):</b> {stats['last_original_bytes']} bytes</p>
    <p><b>Compressed payload size:</b> {stats['last_compressed_bytes']} bytes</p>
    <p><b>Compression ratio:</b> {stats['last_ratio']:.2f}:1</p>
    <p><b>Space saved:</b> {stats.get('last_space_saved', 0):.1f}%</p>
    
    <h3>📦 Raw Payload (Hex)</h3>
    <p style="word-break: break-all; background: white; padding: 5px;">{stats['last_raw_payload'][:200]}{'...' if len(stats['last_raw_payload']) > 200 else ''}</p>
    
    <h3>📋 Decompressed Data Preview</h3>
    <div style="background: white; padding: 5px;">
    {decompressed_preview if decompressed_preview else "No data received yet"}
    </div>
    </div>
    <p><i>Refresh page to see latest data</i></p>
    """

@app.route('/api/inverter/upload', methods=['POST'])
def upload_data():
    try:
        print(f"\n=== CLOUD API RECEIVED REQUEST ===")
        print(f"Content-Type: {request.content_type}")
        print(f"Content-Length: {request.content_length}")
        
        # Handle both JSON and binary uploads
        if request.content_type == 'application/json':
            # JSON format from test_upload.py
            json_data = request.get_json()
            payload_hex = json_data.get('data', '')
            device_id = json_data.get('device_id', 'Unknown')
            data_size = json_data.get('size', 0)
            
            print(f"JSON Upload from device: {device_id}")
            print(f"Reported size: {data_size} bytes")
            print(f"Hex data length: {len(payload_hex)} chars")
            
        else:
            # Binary format from ESP32
            payload_hex = request.data.decode('utf-8').strip()
            device_id = "ESP32_DIRECT"
            data_size = len(payload_hex) // 2
        
        print(f"Raw payload (hex): {payload_hex}")
        
        if not payload_hex:
            print("[ERROR] Empty payload received")
            return jsonify({"status": "ERROR", "message": "Empty payload"}), 400
        
        # Convert hex to bytes
        try:
            compressed_bytes = binascii.unhexlify(payload_hex)
            print(f"Compressed bytes: {len(compressed_bytes)} bytes")
        except Exception as e:
            print(f"[ERROR] Invalid hex data: {e}")
            return jsonify({"status": "ERROR", "message": "Invalid hex data"}), 400

        # Save raw compressed block
        timestamp = int(time.time())
        filename = os.path.join(UPLOAD_FOLDER, f'upload_{device_id}_{timestamp}.bin')
        with open(filename, 'wb') as f:
            f.write(compressed_bytes)
        print(f"💾 Saved to: {filename}")

        # Decompress and analyze
        records = decompress_delta(compressed_bytes)
        num_records = len(records)
        print(f"Decompressed to {num_records} records")

        # Show first few decompressed records
        if records:
            print("First 3 decompressed records:")
            for i, r in enumerate(records[:3]):
                print(f"  Record {i+1}: ts={r['ts_ms']}ms, value={r['value']:.2f}V")

        # Calculate compression stats
        original_bytes_est = num_records * 20  # realistic estimate: timestamp(8) + value(4) + metadata(8)
        compressed_bytes_len = len(compressed_bytes)
        ratio = original_bytes_est / compressed_bytes_len if compressed_bytes_len > 0 else 1
        space_saved = ((original_bytes_est - compressed_bytes_len) / original_bytes_est * 100) if original_bytes_est > 0 else 0

        print(f"Compression Analysis:")
        print(f"  Original size (est): {original_bytes_est} bytes")
        print(f"  Compressed size: {compressed_bytes_len} bytes")
        print(f"  Compression ratio: {ratio:.2f}:1")
        print(f"  Space saved: {space_saved:.1f}%")

        # Update global stats
        stats["uploads"] += 1
        stats["last_num_records"] = num_records
        stats["last_original_bytes"] = original_bytes_est
        stats["last_compressed_bytes"] = compressed_bytes_len
        stats["last_ratio"] = ratio
        stats["last_space_saved"] = space_saved
        stats["last_raw_payload"] = payload_hex
        stats["last_decompressed_data"] = records
        stats["last_upload_time"] = time.strftime('%Y-%m-%d %H:%M:%S')
        stats["device_id"] = json_data.get('device_id', 'Unknown') if request.content_type == 'application/json' else 'ESP32_DIRECT'

        # Respond with detailed JSON feedback
        response = {
            "status": "OK",
            "timestamp": timestamp,
            "received_bytes": compressed_bytes_len,
            "num_records": num_records,
            "original_bytes_est": original_bytes_est,
            "compressed_bytes": compressed_bytes_len,
            "compression_ratio": round(ratio, 2),
            "space_saved_percent": round(space_saved, 1),
            "config": {
                "upload_interval": 15000,  # 15 seconds for demo
                "chunk_size": 32
            },
            "commands": ["ack_received", "continue_monitoring"]
        }
        
        print(f"Sending response: {response}")
        print("================================\n")
        return jsonify(response), 200

    except Exception as e:
        print(f"[ERROR] Exception in upload_data: {e}")
        return jsonify({"status": "ERROR", "message": str(e)}), 400

@app.route('/api/nodered/stats', methods=['GET'])
def nodered_stats():
    """API endpoint for Node-RED dashboard - Current stats"""
    current_stats = {
        "timestamp": int(time.time() * 1000),
        "upload_time": stats.get("last_upload_time", "Never"),
        "device_id": stats.get("device_id", "Unknown"),
        "total_uploads": stats.get("uploads", 0),
        "records": stats.get("last_num_records", 0),
        "original_bytes": stats.get("last_original_bytes", 0),
        "compressed_bytes": stats.get("last_compressed_bytes", 0),
        "compression_ratio": stats.get("last_ratio", 0),
        "space_saved": round(((stats.get("last_original_bytes", 0) - stats.get("last_compressed_bytes", 0)) / max(stats.get("last_original_bytes", 1), 1)) * 100, 1)
    }
    return jsonify(current_stats)

@app.route('/api/nodered/solar-data', methods=['GET'])
def nodered_solar_data():
    """API endpoint for Node-RED dashboard - Solar inverter data"""
    solar_data = []
    records = stats.get("last_decompressed_data", [])
    
    for i, record in enumerate(records[:10]):  # Last 10 records
        solar_data.append({
            "timestamp": record.get("ts_ms", 0),
            "voltage": record.get("voltage", record.get("value", 0)),
            "record_id": i + 1,
            "unit": record.get("unit", "V"),
            "device": record.get("device_id", "ESP32")
        })
    
    return jsonify({
        "data": solar_data,
        "count": len(solar_data),
        "last_update": stats.get("last_upload_time", "Never")
    })

@app.route('/api/nodered/compression-history', methods=['GET'])
def nodered_compression_history():
    """API endpoint for Node-RED dashboard - Compression analytics"""
    # For demo, create some historical data based on current stats
    history = []
    base_time = int(time.time() * 1000)
    
    for i in range(10):  # Last 10 uploads simulation
        history.append({
            "timestamp": base_time - (i * 15000),  # 15 seconds apart
            "upload_id": stats.get("uploads", 0) - i,
            "compression_ratio": stats.get("last_ratio", 0) + (i * 0.1),
            "original_bytes": stats.get("last_original_bytes", 0) + (i * 10),
            "compressed_bytes": stats.get("last_compressed_bytes", 0) + (i * 2),
            "records_count": stats.get("last_num_records", 0),
            "space_saved": round(85 + (i * 0.5), 1)  # Demo variation
        })
    
    return jsonify({
        "history": list(reversed(history)),  # Chronological order
        "total_uploads": stats.get("uploads", 0),
        "avg_compression": round(sum(h["compression_ratio"] for h in history) / len(history), 2) if history else 0
    })

if __name__ == '__main__':
    print(" EcoWatt Cloud API Server Starting...")
    print(" Dashboard: http://localhost:5000/")
    print(" API Endpoint: http://localhost:5000/api/inverter/upload")
    app.run(host='0.0.0.0', port=5000, debug=False)
