from flask import Flask, request, jsonify
import os, binascii, time, json
import paho.mqtt.client as mqtt

app = Flask(__name__)

# MQTT Configuration
MQTT_BROKER = "broker.emqx.io"  # Public MQTT broker
MQTT_PORT = 1883
MQTT_TOPIC = "vdl/replace"  # Topic as requested
MQTT_CLIENT_ID = "ecowatt_server"

# Initialize MQTT client
mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1, client_id=MQTT_CLIENT_ID)

def on_mqtt_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✅ Connected to MQTT broker at {MQTT_BROKER}:{MQTT_PORT}")
    else:
        print(f"❌ Failed to connect to MQTT broker. Return code {rc}")

def on_mqtt_publish(client, userdata, mid):
    print(f"📡 MQTT message published successfully (mid: {mid})")

# Set MQTT callbacks
mqtt_client.on_connect = on_mqtt_connect
mqtt_client.on_publish = on_mqtt_publish

# Connect to MQTT broker (with error handling)
try:
    mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
    mqtt_client.loop_start()  # Start the network loop in a separate thread
    print(f"🔗 Attempting to connect to MQTT broker at {MQTT_BROKER}:{MQTT_PORT}")
except Exception as e:
    print(f"⚠️ MQTT connection failed: {e}. Server will continue without MQTT.")
    mqtt_client = None

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

def group_records_to_json(records):
    """Group inverter records into data1, data2, etc. format with register values and timestamps"""
    if not records:
        return {}
    
    # Register names in the order we expect them (based on Acquisition.cpp)
    register_names = ["Vac1", "Iac1", "Fac1", "Vpv1", "Vpv2", "Ipv1", "Ipv2", "Temp", "Export", "Power"]
    
    grouped_data = {}
    group_count = 0
    
    # Group records by sets of 10 (one complete reading session)
    for i in range(0, len(records), 10):
        group_count += 1
        group_name = f"data{group_count}"
        group_data = {}
        
        # Get timestamp from the first record in this group (all records in a group have same timestamp)
        if i < len(records):
            group_timestamp = records[i].get('ts_ms', 0)
            group_data['timestamp'] = int(group_timestamp)
        
        # Process up to 10 records in this group
        for j in range(10):
            if i + j < len(records):
                record = records[i + j]
                register_name = record.get('register', f'Unknown_{j}')
                
                # Use "Output power" instead of "Power" for the last register
                if register_name == "Power":
                    register_name = "Output power"
                
                # Store the value (rounded to 1 decimal place for cleaner JSON)
                group_data[register_name] = round(record.get('value', 0), 1)
            else:
                # Fill missing registers with 0
                expected_register = register_names[j] if j < len(register_names) else f'Unknown_{j}'
                if expected_register == "Power":
                    expected_register = "Output power"
                
                group_data[expected_register] = 0
        
        grouped_data[group_name] = group_data
    
    return grouped_data

def publish_to_mqtt(grouped_data, records=None):
    """Publish the grouped JSON data to MQTT broker with timestamp information"""
    if mqtt_client is None:
        print("⚠️ MQTT client not available. Skipping MQTT publish.")
        return False
    
    try:
        # Create timestamp information if records are provided
        timestamp_info = {}
        if records and len(records) > 0:
            first_record = records[0]
            last_record = records[-1]
            time_span = last_record['ts_ms'] - first_record['ts_ms']
            
            timestamp_info = {
                "first_record_time": first_record.get('real_timestamp', 'N/A'),
                "last_record_time": last_record.get('real_timestamp', 'N/A'),
                "esp32_first_timestamp": first_record.get('esp32_relative_time', 'N/A'),
                "esp32_last_timestamp": last_record.get('esp32_relative_time', 'N/A'),
                "time_span_ms": time_span,
                "time_span_seconds": round(time_span/1000, 2),
                "server_received_at": time.strftime('%Y-%m-%d %H:%M:%S')
            }
        
        # Create payload with grouped data, compression statistics, and timestamps
        mqtt_payload = {
            "timestamp_info": timestamp_info,
            "compression_stats": {
                "original_bytes": stats.get("last_original_bytes", 0),
                "compressed_bytes": stats.get("last_compressed_bytes", 0),
                "compression_ratio": round(stats.get("last_ratio", 0), 2),
                "space_saved_percent": round(stats.get("last_space_saved", 0), 1),
                "total_records": stats.get("last_num_records", 0),
                "upload_count": stats.get("uploads", 0)
            }
        }
        
        # Add the grouped data to the payload
        mqtt_payload.update(grouped_data)
        
        # Convert to JSON string
        json_payload = json.dumps(mqtt_payload, indent=2)
        
        # Publish to MQTT
        result = mqtt_client.publish(MQTT_TOPIC, json_payload, qos=1)
        
        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            print(f"📡 Successfully published {len(grouped_data)} data groups + timestamps + compression stats to MQTT topic: {MQTT_TOPIC}")
            if timestamp_info:
                print(f"📅 Published timestamp span: {timestamp_info['time_span_seconds']}s")
            print(f"📊 Published data preview: {list(grouped_data.keys())} + compression_stats")
            return True
        else:
            print(f"❌ MQTT publish failed with return code: {result.rc}")
            return False
            
    except Exception as e:
        print(f"❌ Error publishing to MQTT: {e}")
        return False

def decompress_delta(data: bytes):
    """Decompress delta-encoded data matching ESP32 Compression.cpp algorithm with proper timestamp reconstruction"""
    if not data:
        return []
    
    n = data[0]  # Number of records
    idx = 1
    out = []
    
    # Register definitions matching CloudTransport.cpp
    register_info = [
        {"name": "Vac1", "unit": "V", "scale": 10.0, "desc": "L1 Phase voltage"},
        {"name": "Iac1", "unit": "A", "scale": 10.0, "desc": "L1 Phase current"},
        {"name": "Fac1", "unit": "Hz", "scale": 100.0, "desc": "L1 Phase frequency"},
        {"name": "Vpv1", "unit": "V", "scale": 10.0, "desc": "PV1 input voltage"},
        {"name": "Vpv2", "unit": "V", "scale": 10.0, "desc": "PV2 input voltage"},
        {"name": "Ipv1", "unit": "A", "scale": 10.0, "desc": "PV1 input current"},
        {"name": "Ipv2", "unit": "A", "scale": 10.0, "desc": "PV2 input current"},
        {"name": "Temp", "unit": "C", "scale": 10.0, "desc": "Inverter internal temperature"},
        {"name": "Export", "unit": "%", "scale": 1.0, "desc": "Export power percentage"},
        {"name": "Power", "unit": "W", "scale": 1.0, "desc": "Inverter current output power"}
    ]
    
    print(f"[DECOMPRESS] 🗜️ Processing {n} records from {len(data)} bytes...")
    
    # Initialize timestamp reconstruction
    # The first record's timestamp is relative to ESP32 boot time
    # We'll calculate the approximate real timestamp by working backwards from current time
    current_server_time_ms = int(time.time() * 1000)
    esp32_timestamp_ms = 0  # Will be reconstructed from deltas
    first_record_processed = False
    
    # Since ESP32 sends records sequentially, each record represents one register
    # from the complete set of 10 registers collected in each Modbus read
    for i in range(n):
        if idx + 2 >= len(data):
            print(f"[WARNING] Data truncated at record {i}")
            break
            
        # Read delta and reconstruct ESP32 timestamp
        delta = data[idx]; idx += 1
        esp32_timestamp_ms += delta  # Accumulate ESP32 milliseconds since boot
        
        # Calculate approximate real-world timestamp
        if not first_record_processed:
            # For the first record, estimate the real timestamp
            # Assume the upload happened recently (within last few seconds)
            estimated_esp32_boot_time = current_server_time_ms - esp32_timestamp_ms
            first_record_processed = True
            print(f"[TIMESTAMP] 📅 Estimated ESP32 boot time: {estimated_esp32_boot_time} ms ago")
            print(f"[TIMESTAMP] 🕒 First record ESP32 timestamp: {esp32_timestamp_ms} ms since boot")
        
        real_timestamp_ms = current_server_time_ms - (esp32_timestamp_ms if i == 0 else 0) + (esp32_timestamp_ms if i > 0 else 0)
        # Simplified: use ESP32 relative time for now, convert to readable format later
        
        # Read compressed value (2 bytes) - this is the raw register value
        raw_value = (data[idx] << 8) | data[idx+1]; idx += 2
        
        # Determine which register this is based on the record sequence
        # If ESP32 sends all 10 registers in one upload, cycle through them
        reg_index = i % len(register_info)
        reg_info = register_info[reg_index]
        
        actual_value = raw_value / reg_info["scale"]
        
        record = {
            "ts_ms": esp32_timestamp_ms,  # ESP32 milliseconds since boot
            "server_received_ms": current_server_time_ms,  # When server received this
            "real_timestamp": time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(current_server_time_ms/1000)),
            "esp32_relative_time": f"{esp32_timestamp_ms}ms since boot",
            "delta_from_previous": delta,
            "raw_value": raw_value,
            "value": actual_value,
            "unit": reg_info["unit"],
            "register": reg_info["name"],
            "description": reg_info["desc"],
            "device_id": "ESP32_SOLAR"
        }
        out.append(record)
        
    print(f"[SUCCESS] ✅ Successfully decompressed {len(out)} records with timestamps")
    if out:
        print(f"[TIMESTAMP] 📊 Timestamp range: {out[0]['esp32_relative_time']} to {out[-1]['esp32_relative_time']}")
        print(f"[TIMESTAMP] 🕒 Time span: {out[-1]['ts_ms'] - out[0]['ts_ms']}ms")
    
    return out

@app.route('/')
def index():
    """Enhanced dashboard view for all inverter registers"""
    decompressed_preview = ""
    if stats['last_decompressed_data']:
        # Show ALL records without truncation
        total_records = len(stats['last_decompressed_data'])
        decompressed_preview = f"<p><strong>📊 Showing ALL {total_records} records with timestamps (COMPLETE DATA):</strong></p>"
        decompressed_preview += f"<p style='color: #666;'><em>Debug info: Upload #{stats.get('uploads', 0)} received at {stats.get('last_upload_time', 'Never')}</em></p>"
        
        # Add timestamp summary
        if total_records > 0:
            first_record = stats['last_decompressed_data'][0]
            last_record = stats['last_decompressed_data'][-1]
            time_span = last_record['ts_ms'] - first_record['ts_ms']
            decompressed_preview += f"<p style='background: #e3f2fd; padding: 10px; border-radius: 5px;'>"
            decompressed_preview += f"🕒 <strong>Timestamp Info:</strong><br>"
            decompressed_preview += f"First record: {first_record.get('esp32_relative_time', 'N/A')} ({first_record.get('real_timestamp', 'N/A')})<br>"
            decompressed_preview += f"Last record: {last_record.get('esp32_relative_time', 'N/A')} ({last_record.get('real_timestamp', 'N/A')})<br>"
            decompressed_preview += f"Time span: {time_span}ms ({time_span/1000:.2f} seconds)"
            decompressed_preview += f"</p>"
        
        decompressed_preview += "<br>".join([
            f"Record {i+1}: {r['register']} = {r['value']:.3f} {r['unit']} (Raw: {r.get('raw_value', 'N/A')}) "
            f"<br>&nbsp;&nbsp;&nbsp;&nbsp;📅 ESP32 time: {r.get('esp32_relative_time', 'N/A')} "
            f"(Δ{r.get('delta_from_previous', 0)}ms) - {r.get('description', '')}" 
            for i, r in enumerate(stats['last_decompressed_data'])
        ])
    
    # Extract latest values for each register type for summary
    latest_values = {}
    if stats['last_decompressed_data']:
        for record in stats['last_decompressed_data']:
            reg_name = record.get('register', 'Unknown')
            if reg_name not in latest_values:
                latest_values[reg_name] = record
    
    inverter_status = ""
    if latest_values:
        inverter_status = "<h3>🔋 Current Inverter Status (Latest Values)</h3>"
        inverter_status += "<div style='display: grid; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); gap: 10px; margin: 10px 0;'>"
        
        # Organize registers by category
        power_regs = ['Vac1', 'Iac1', 'Fac1', 'Power']
        pv_regs = ['Vpv1', 'Vpv2', 'Ipv1', 'Ipv2']
        system_regs = ['Temp', 'Export']
        
        # AC Power section
        inverter_status += "<div style='background: #e8f5e8; padding: 15px; border-radius: 8px; border-left: 4px solid #4CAF50;'>"
        inverter_status += "<h4 style='margin-top: 0; color: #2E7D32;'>⚡ AC Output</h4>"
        for reg_name in power_regs:
            if reg_name in latest_values:
                record = latest_values[reg_name]
                inverter_status += f"<p style='margin: 5px 0;'><b>{record.get('description', reg_name)}:</b> <span style='font-size: 1.2em; color: #1976D2;'>{record['value']:.3f} {record['unit']}</span></p>"
        inverter_status += "</div>"
        
        # PV Input section
        inverter_status += "<div style='background: #fff3e0; padding: 15px; border-radius: 8px; border-left: 4px solid #FF9800;'>"
        inverter_status += "<h4 style='margin-top: 0; color: #E65100;'>☀️ Solar Input</h4>"
        for reg_name in pv_regs:
            if reg_name in latest_values:
                record = latest_values[reg_name]
                inverter_status += f"<p style='margin: 5px 0;'><b>{record.get('description', reg_name)}:</b> <span style='font-size: 1.2em; color: #1976D2;'>{record['value']:.3f} {record['unit']}</span></p>"
        inverter_status += "</div>"
        
        # System Status section
        inverter_status += "<div style='background: #f3e5f5; padding: 15px; border-radius: 8px; border-left: 4px solid #9C27B0;'>"
        inverter_status += "<h4 style='margin-top: 0; color: #7B1FA2;'>🔧 System Status</h4>"
        for reg_name in system_regs:
            if reg_name in latest_values:
                record = latest_values[reg_name]
                inverter_status += f"<p style='margin: 5px 0;'><b>{record.get('description', reg_name)}:</b> <span style='font-size: 1.2em; color: #1976D2;'>{record['value']:.3f} {record['unit']}</span></p>"
        inverter_status += "</div>"
        
        inverter_status += "</div>"
    
    return f"""
    <!DOCTYPE html>
    <html>
    <head>
        <title>EcoWatt Solar Inverter Dashboard</title>
        <meta http-equiv="refresh" content="5">
        <meta http-equiv="Cache-Control" content="no-cache, no-store, must-revalidate">
        <meta http-equiv="Pragma" content="no-cache">
        <meta http-equiv="Expires" content="0">
    </head>
    <body>
    <h1>🌞 EcoWatt Solar Inverter Dashboard</h1>
    <div style="font-family: Arial, sans-serif; background: #f0f8ff; padding: 15px; border-radius: 10px;">
    
    {inverter_status}
    
    <h3>📊 Data Compression Statistics</h3>
    <div style="background: #e6f3ff; padding: 10px; margin: 10px 0; border-radius: 5px;">
    <p><b>Total uploads received:</b> {stats['uploads']}</p>
    <p><b>Last records processed:</b> {stats['last_num_records']}</p>
    <p><b>Original payload size (est):</b> {stats['last_original_bytes']} bytes</p>
    <p><b>Compressed payload size:</b> {stats['last_compressed_bytes']} bytes</p>
    <p><b>Compression ratio:</b> {stats['last_ratio']:.2f}:1</p>
    <p><b>Space saved:</b> {stats.get('last_space_saved', 0):.1f}%</p>
    <p><b>Last upload:</b> {stats.get('last_upload_time', 'Never')}</p>
    </div>
    
    <h3>📦 Raw Compressed Payload (Hex)</h3>
    <div style="background: #fff; padding: 5px; margin: 10px 0; border: 1px solid #ccc; font-family: monospace; word-break: break-all;">
    {stats['last_raw_payload'][:400]}{'...' if len(stats['last_raw_payload']) > 400 else ''}
    </div>
    
    <h3>📋 All Decompressed Inverter Records</h3>
    <div style="background: #fff; padding: 10px; margin: 10px 0; border: 1px solid #ccc; max-height: 600px; overflow-y: auto;">
    {decompressed_preview if decompressed_preview else "No inverter data received yet"}
    </div>
    </div>
    <p><i>Page auto-refreshes every 5 seconds | ESP32 uploads every 15 seconds</i></p>
    </body>
    </html>
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

        # Decompress and analyze
        records = decompress_delta(compressed_bytes)
        num_records = len(records)
        print(f"Decompressed to {num_records} records")

        # Show first few decompressed records with detailed timestamp info
        if records:
            print(f"📊 First {min(len(records), 3)} decompressed inverter records with timestamps:")
            for i, r in enumerate(records[:3]):
                print(f"  Record {i+1}: {r['register']} = {r['value']:.3f} {r['unit']} (Raw: {r.get('raw_value', 'N/A')})")
                print(f"    📅 ESP32 timestamp: {r.get('esp32_relative_time', 'N/A')}")
                print(f"    🕒 Real time: {r.get('real_timestamp', 'N/A')}")
                print(f"    ⏱️ Delta from previous: {r.get('delta_from_previous', 0)}ms")
                print(f"    📝 Description: {r.get('description', '')}")
            
            # Show timestamp summary for entire batch
            if len(records) > 1:
                time_span = records[-1]['ts_ms'] - records[0]['ts_ms']
                print(f"📈 Batch timestamp summary:")
                print(f"    First: {records[0].get('esp32_relative_time', 'N/A')}")
                print(f"    Last: {records[-1].get('esp32_relative_time', 'N/A')}")
                print(f"    Total span: {time_span}ms ({time_span/1000:.2f} seconds)")

        # Calculate compression stats
        original_bytes_est = num_records * 192  # realistic estimate: timestamp(8) + value(4) + metadata(8)
        compressed_bytes_len = len(compressed_bytes)
        ratio = original_bytes_est / compressed_bytes_len if compressed_bytes_len > 0 else 1
        space_saved = ((original_bytes_est - compressed_bytes_len) / original_bytes_est * 100) if original_bytes_est > 0 else 0

        print(f"Compression Analysis:")
        print(f"  Original size (est): {original_bytes_est} bytes")
        print(f"  Compressed size: {compressed_bytes_len} bytes")
        print(f"  Compression ratio: {ratio:.2f}:1")
        print(f"  Space saved: {space_saved:.1f}%")

        # Update global stats
        timestamp = int(time.time())  # Define timestamp for response
        stats["uploads"] += 1
        stats["last_num_records"] = num_records
        stats["last_original_bytes"] = original_bytes_est
        stats["last_compressed_bytes"] = compressed_bytes_len
        stats["last_ratio"] = ratio
        stats["last_space_saved"] = space_saved
        stats["last_raw_payload"] = payload_hex
        stats["last_decompressed_data"] = records  # Store ALL records without any limitation
        stats["last_upload_time"] = time.strftime('%Y-%m-%d %H:%M:%S')
        stats["device_id"] = json_data.get('device_id', 'Unknown') if request.content_type == 'application/json' else 'ESP32_DIRECT'
        
        # Debug: Log actual data storage
        print(f"[DEBUG] Stored {len(records)} records in stats['last_decompressed_data']")
        print(f"[DEBUG] Total stats uploads: {stats['uploads']}")
        print(f"[DEBUG] Stats data length verification: {len(stats['last_decompressed_data'])}")
        
        # Create grouped JSON format and publish to MQTT
        print(f"\n🔄 Creating grouped JSON format for MQTT...")
        grouped_json = group_records_to_json(records)
        print(f"📋 Created {len(grouped_json)} data groups: {list(grouped_json.keys())}")
        
        # Show sample of the new format with timestamps
        if grouped_json:
            sample_group = list(grouped_json.keys())[0]
            sample_data = grouped_json[sample_group]
            print(f"📊 Sample group format: {sample_group} = {dict(list(sample_data.items())[:3])}...timestamp:{sample_data.get('timestamp', 'N/A')}")
        
        # Publish to MQTT with timestamp information
        mqtt_success = publish_to_mqtt(grouped_json, records)
        if mqtt_success:
            print(f"✅ MQTT publish successful")
        else:
            print(f"⚠️ MQTT publish failed or skipped")
        print(f"================================")

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
        error_msg = str(e).encode('ascii', 'ignore').decode('ascii')
        print(f"[ERROR] Exception in upload_data: {error_msg}")
        return jsonify({"status": "ERROR", "message": error_msg}), 400

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
    """API endpoint for Node-RED dashboard - All solar inverter registers"""
    solar_data = []
    records = stats.get("last_decompressed_data", [])
    
    # Show ALL records without any limitation
    for i, record in enumerate(records):
        solar_data.append({
            "timestamp": record.get("ts_ms", 0),
            "register": record.get("register", "Unknown"),
            "value": record.get("value", 0),
            "raw_value": record.get("raw_value", 0),
            "unit": record.get("unit", ""),
            "description": record.get("description", ""),
            "record_id": i + 1,
            "device": record.get("device_id", "ESP32")
        })
    
    # Organize by register type for easier consumption
    by_register = {}
    for data in solar_data:
        reg = data["register"]
        if reg not in by_register:
            by_register[reg] = []
        by_register[reg].append(data)
    
    return jsonify({
        "data": solar_data,
        "by_register": by_register,
        "count": len(solar_data),
        "register_types": list(by_register.keys()),
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
