from flask import Flask, request, jsonify
import os, binascii

app = Flask(__name__)
UPLOAD_FOLDER = 'uploads'
os.makedirs(UPLOAD_FOLDER, exist_ok=True)

# --- Global state for demo dashboard ---
stats = {
    "uploads": 0,
    "last_num_records": 0,
    "last_original_bytes": 0,
    "last_compressed_bytes": 0,
    "last_ratio": 0.0
}

def decompress_delta(data: bytes):
    if not data:
        return []
    n = data[0]
    ts = 0
    idx = 1
    out = []
    for i in range(n):
        if idx + 2 >= len(data):
            break
        delta = data[idx]; idx += 1
        ts += delta
        vi = (data[idx] << 8) | data[idx+1]; idx += 2
        v = vi / 100.0
        out.append({"ts_ms": ts, "value": v})
    return out

@app.route('/')
def index():
    """Simple dashboard view in browser"""
    return f"""
    <h1>EcoWatt Server Dashboard</h1>
    <p><b>Total uploads:</b> {stats['uploads']}</p>
    <p><b>Last records:</b> {stats['last_num_records']}</p>
    <p><b>Original bytes est:</b> {stats['last_original_bytes']}</p>
    <p><b>Compressed bytes:</b> {stats['last_compressed_bytes']}</p>
    <p><b>Compression ratio:</b> {stats['last_ratio']:.2f}</p>
    """

@app.route('/api/inverter/upload', methods=['POST'])
def upload_data():
    try:
        # ESP sends payload as hex string
        payload_hex = request.data.decode('utf-8').strip()
        compressed_bytes = binascii.unhexlify(payload_hex)

        # Save raw compressed block
        filename = os.path.join(
            UPLOAD_FOLDER,
            f'upload_{request.remote_addr}_{request.environ.get("REMOTE_PORT","unknown")}.bin'
        )
        with open(filename, 'wb') as f:
            f.write(compressed_bytes)

        # Decompress
        records = decompress_delta(compressed_bytes)
        num_records = len(records)

        # Calculate compression stats
        original_bytes_est = num_records * 16  # assume ~16 bytes per record
        compressed_bytes_len = len(compressed_bytes)
        ratio = compressed_bytes_len / original_bytes_est if original_bytes_est else 1

        # Update global stats
        stats["uploads"] += 1
        stats["last_num_records"] = num_records
        stats["last_original_bytes"] = original_bytes_est
        stats["last_compressed_bytes"] = compressed_bytes_len
        stats["last_ratio"] = ratio

        print(f"[SERVER] Received {num_records} records "
              f"(compressed {compressed_bytes_len}B, est original {original_bytes_est}B, ratio={ratio:.2f})")

        # Respond with JSON feedback
        response = {
            "status": "OK",
            "num_records": num_records,
            "original_bytes_est": original_bytes_est,
            "compressed_bytes": compressed_bytes_len,
            "compression_ratio": ratio,
            "config": {"upload_interval": 900000},
            "commands": ["noop"]
        }
        return jsonify(response), 200

    except Exception as e:
        return jsonify({"status": "ERROR", "message": str(e)}), 400

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=8080)
