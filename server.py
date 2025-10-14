#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os, time, json, hmac, base64, hashlib, struct, threading, tempfile
from typing import Dict, Any, List, Tuple
from flask import Flask, request, jsonify
import paho.mqtt.client as mqtt
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

# =================== CONFIG ===================
BROKER_HOST = "broker.emqx.io"      # or "localhost"
BROKER_PORT = 1883
MQTT_USER   = None                   # set if needed
MQTT_PASS   = None

DEV_ID      = "esp32-01"             # MUST match device's sec.begin(...)
TOPIC_CMD   = f"devices/{DEV_ID}/fota/cmd"       # downlink (server -> device, FOTA)
TOPIC_STAT  = f"devices/{DEV_ID}/fota/status"    # uplink status (device -> server)
TOPIC_DATA  = f"devices/{DEV_ID}/data/dulmin"           # uplink telemetry (device -> server)
TOPIC_CONFIG= f"devices/{DEV_ID}/config"         # downlink config (server -> device)
TOPIC_ACK   = f"devices/{DEV_ID}/ack"            # uplink acks for config / misc
TOPIC_WRITE = f"devices/{DEV_ID}/write"          # downlink write (server -> device)

# 32-byte PSK hex — must match ESP32 provisioning
PSK_HEX     = "4968A7E8835BC6EC5BDBE15AA9E7C478E5616E33AA0CC4CADB53A81AA20FA727"

DEFAULT_CHUNK = 4096
MAX_EVENTS    = 200
MAX_RECORDS   = 1000
# ==============================================

app = Flask(__name__)
app.config['MAX_CONTENT_LENGTH'] = 64 * 1024 * 1024  # 64MB upload cap

# ---------- SecureLink helpers ----------
HDR_LEN = 1 + 1 + 2 + 8 + 8 + 12  # ver|type|reserved|boot|seq|iv12
MAC_LEN = 32

def hkdf48(ikm: bytes, salt: bytes, info: bytes) -> bytes:
    prk = hmac.new(salt, ikm, hashlib.sha256).digest()
    T = b""; okm = b""; ctr = 0
    while len(okm) < 48:
        ctr += 1
        T = hmac.new(prk, T + info + bytes([ctr]), hashlib.sha256).digest()
        okm += T
    return okm[:48]

def derive_keys(device_id: str, psk_hex: str) -> Tuple[bytes, bytes]:
    salt = hashlib.sha256(device_id.encode("utf-8")).digest()
    okm  = hkdf48(bytes.fromhex(psk_hex), salt, b"EcoWatt")
    return okm[:16], okm[16:48]   # Kenc, Kmac

def aes_ctr_crypt(Kenc: bytes, iv12: bytes, data: bytes) -> bytes:
    ctr16 = iv12 + b"\x00\x00\x00\x00"
    c = Cipher(algorithms.AES(Kenc), modes.CTR(ctr16)).encryptor()
    return c.update(data) + c.finalize()

# sealed command publisher (downlink), per-device anti-replay
downlink: Dict[str, Dict[str, int]] = {}  # deviceId -> {"boot": int(ms), "seq": int}

def seal_downlink(obj: Dict[str, Any], msg_type: int = 2, device_id: str = DEV_ID) -> str:
    Kenc, Kmac = derive_keys(device_id, PSK_HEX)
    st = downlink.get(device_id, {"boot": 0, "seq": 0})
    if st["boot"] == 0:
        # ensure clearly "newer" than any previous sender
        st["boot"] = int(time.time() * 1000) + 10_000_000
        st["seq"]  = 0
    st["seq"] += 1
    downlink[device_id] = st

    iv12 = os.urandom(12)
    hdr  = struct.pack("<BBHQQ", 1, msg_type & 0xFF, 0, st["boot"], st["seq"]) + iv12
    plain = json.dumps(obj, separators=(",", ":")).encode("utf-8")
    ct  = aes_ctr_crypt(Kenc, iv12, plain)
    mac = hmac.new(Kmac, hdr + ct, hashlib.sha256).digest()
    return base64.b64encode(hdr + ct + mac).decode("ascii")

def try_open_uplink(sealed_b64: bytes) -> Dict[str, Any] | None:
    try:
        raw = base64.b64decode(sealed_b64, validate=True)
    except Exception:
        try: return json.loads(sealed_b64.decode("utf-8"))
        except Exception: return None

    if len(raw) < HDR_LEN + MAC_LEN: return None
    hdr = raw[:HDR_LEN]; ct = raw[HDR_LEN:-MAC_LEN]; mac = raw[-MAC_LEN:]
    Kenc, Kmac = derive_keys(DEV_ID, PSK_HEX)
    tag = hmac.new(Kmac, hdr + ct, hashlib.sha256).digest()
    if not hmac.compare_digest(tag, mac): return None
    iv12 = hdr[-12:]
    plain = aes_ctr_crypt(Kenc, iv12, ct)  # CTR decrypt
    try: return json.loads(plain.decode("utf-8"))
    except Exception: return None

# ---------- Delta decompressor ----------
def get16_le(buf: bytes, i: int):
    if i + 2 > len(buf): return None
    return buf[i] | (buf[i+1] << 8)
def get32_le(buf: bytes, i: int):
    if i + 4 > len(buf): return None
    return (buf[i] | (buf[i+1]<<8) | (buf[i+2]<<16) | (buf[i+3]<<24)) & 0xFFFFFFFF
def get64_le(buf: bytes, i: int):
    if i + 8 > len(buf): return None
    low = get32_le(buf, i); high = get32_le(buf, i+4)
    return low + (high * 0x100000000)

REGISTER_MAP = {
    0:  ('Vac1_L1_Phase_voltage',          10, 'V'),
    1:  ('Iac1_L1_Phase_current',          10, 'A'),
    2:  ('Fac1_L1_Phase_frequency',       100, 'Hz'),
    3:  ('Vpv1_PV1_input_voltage',         10, 'V'),
    4:  ('Vpv2_PV2_input_voltage',         10, 'V'),
    5:  ('Ipv1_PV1_input_current',         10, 'A'),
    6:  ('Ipv2_PV2_input_current',         10, 'A'),
    7:  ('Inverter_internal_temperature',  10, '°C'),
    8:  ('Set_export_power_percentage',     1, '%'),
    9:  ('Pac_L_Inverter_current_output_power', 1, 'W'),
}
def decode_register(addr: int, raw: int) -> Dict[str, Any]:
    if addr in REGISTER_MAP:
        name, gain, unit = REGISTER_MAP[addr]
        return {"address": addr, "name": name, "raw_value": raw, "value": raw / gain, "unit": unit}
    return {"address": addr, "name": f"Unknown_Register_{addr}", "raw_value": raw, "value": raw, "unit": "unknown"}
def decompress_delta(hex_string: str):
    buf = bytes.fromhex(hex_string); i = 0
    base_ts = get64_le(buf, i); i += 8
    if base_ts is None: return None
    nrecs   = get16_le(buf, i); i += 2
    if nrecs is None: return None
    out = []; prev_ts = base_ts
    for _ in range(nrecs):
        dt_ms = get32_le(buf, i); i += 4
        qty   = get16_le(buf, i); i += 2
        if dt_ms is None or qty is None: return None
        need = qty * 4
        if i + need > len(buf): return None
        ts_ms = prev_ts + dt_ms
        regs: Dict[str, Any] = {}
        for __ in range(qty):
            addr = get16_le(buf, i); val  = get16_le(buf, i+2)
            if addr is None or val is None: return None
            d = decode_register(addr, val)
            regs[d["name"]] = {"address": d["address"], "raw_value": d["raw_value"], "value": d["value"], "unit": d["unit"]}
            i += 4
        out.append({"timestamp": ts_ms, "register_count": qty, "registers": regs})
        prev_ts = ts_ms
    return out

# ---------- State ----------
fota_events: List[Dict[str, Any]] = []
data_records: List[Dict[str, Any]] = []

def push_event(ev: Dict[str, Any]):
    fota_events.append({"ts": int(time.time()*1000), **ev})
    if len(fota_events) > MAX_EVENTS:
        del fota_events[:len(fota_events)-MAX_EVENTS]

current_config = {  # server-side cache; update as you like
    "poll_period_ms": 10000,
    "upload_period_ms": 20000,
    "buffer_capacity": 256,
    "reg_req_id_1": 1023
}

# ---------- MQTT ----------
mqttc = mqtt.Client()
if MQTT_USER and MQTT_PASS:
    mqttc.username_pw_set(MQTT_USER, MQTT_PASS)

def on_connect(client, userdata, flags, rc):
    client.subscribe([(TOPIC_STAT, 1), (TOPIC_DATA, 1), (TOPIC_ACK, 1)])
    print(f"[MQTT] rc={rc}; subs: {TOPIC_STAT}, {TOPIC_DATA}, {TOPIC_ACK}")

def on_message(client, userdata, msg):
    if msg.topic == TOPIC_STAT:
        evt = try_open_uplink(msg.payload)
        if not evt:
            print("[FOTA][srv] could not parse status")
            return
        push_event({"topic": "fota/status", **evt})
        ev = evt.get("ev")
        if ev == "need_chunks":
            state["next_offset"] = int(evt.get("next_offset", 0))
            state["total"]       = int(evt.get("total", 0))
            _send_next_chunk()
        elif ev == "progress":
            state["next_offset"] = int(evt.get("next_offset", 0))
            _send_next_chunk()

    elif msg.topic == TOPIC_DATA:
        obj = try_open_uplink(msg.payload)
        if not obj:
            print("[DATA][srv] could not parse data packet")
            return
        if "payload_hex" in obj:
            recs = decompress_delta(obj["payload_hex"])
            if recs is not None:
                data_records.extend(recs)
                if len(data_records) > MAX_RECORDS:
                    del data_records[:len(data_records)-MAX_RECORDS]
                push_event({"topic": "data", "info": f"decoded {len(recs)} records"})
        else:
            push_event({"topic": "data", "info": "rx json", "preview": obj})

    elif msg.topic == TOPIC_ACK:
        try: text = msg.payload.decode("utf-8", "ignore")
        except: text = "<bin>"
        push_event({"topic": "ack", "ack": text})

mqttc.on_connect = on_connect
mqttc.on_message = on_message
mqtt_thr = threading.Thread(target=lambda: mqttc.connect(BROKER_HOST, BROKER_PORT) or mqttc.loop_forever(), daemon=True)
mqtt_thr.start()

# ---------- FOTA ----------
state = {
    "firmware": None, "version": None, "chunk": DEFAULT_CHUNK,
    "sha_hex": None, "nonce": None, "next_offset": 0, "total": 0, "active": False
}
def publish_cmd(obj: Dict[str, Any], msg_type: int = 2):
    mqttc.publish(TOPIC_CMD, seal_downlink(obj, msg_type=msg_type), qos=1, retain=False)
def publish_config(obj: Dict[str, Any]):
    mqttc.publish(TOPIC_CONFIG, seal_downlink(obj, msg_type=2), qos=1, retain=False)
def publish_write(obj: Dict[str, Any]):
    mqttc.publish(TOPIC_WRITE, seal_downlink(obj, msg_type=2), qos=1, retain=False)

def _send_next_chunk():
    if not state["active"]:
        return
    off = state["next_offset"]; total = state["total"]; fw = state["firmware"]; chunk = state["chunk"]
    if off >= total:
        publish_cmd({"op": "finish"})
        push_event({"topic":"fota/status","ev":"finish_sent"})
        return
    end = min(off + chunk, total)
    buf = fw[off:end]
    csha = hashlib.sha256(buf).digest()
    publish_cmd({
        "op": "chunk",
        "offset": int(off),
        "data_b64": base64.b64encode(buf).decode("ascii"),
        "sha256_b64": base64.b64encode(csha).decode("ascii")
    })

# =================== WEB UI ===================

INDEX_HTML = """
<!doctype html>
<html>
<head>
  <meta charset="utf-8"/>
  <title>ESP32 Console — FOTA, Config, Write & Telemetry</title>
  <meta name="viewport" content="width=device-width,initial-scale=1"/>
  <style>
    body{font-family: ui-sans-serif,system-ui,-apple-system,Segoe UI; margin: 2rem; color:#111; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh;}
    h1{margin:0 0 1rem 0; color: white; text-shadow: 0 2px 4px rgba(0,0,0,0.3);}
    .grid{display:grid; grid-template-columns: 1fr 1fr; gap:16px}
    @media(max-width: 1200px){ .grid{grid-template-columns: 1fr} }
    .card{border:1px solid #ddd; border-radius:12px; padding:16px; margin-bottom:16px; box-shadow:0 4px 8px rgba(0,0,0,0.1); background: white;}
    button{padding:8px 14px; border-radius:10px; border:1px solid #ccc; background:#fafafa; cursor:pointer; transition: all 0.2s;}
    button:hover{background:#4a90e2; color: white; transform: translateY(-1px);}
    table{border-collapse: collapse; width:100%;}
    th,td{border-bottom:1px solid #eee; padding:8px; text-align:left; vertical-align:top}
    .muted{color:#666; font-size:12px}
    input[type=file], input[type=text], input[type=number]{padding:8px; border-radius: 6px; border: 1px solid #ddd;}
    progress{width: 220px;}
    code{background:#f6f8fa; padding:2px 6px; border-radius:6px}
    .kv{display:flex; gap:10px; flex-wrap:wrap; align-items:center}
    .kv label{display:flex; align-items:center; gap:6px}
    .stack{display:flex; flex-direction:column; gap:8px}

    /* Solar Dashboard Styles */
    .status-grid{display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 12px; margin: 16px 0;}
    .status-card{padding: 16px; border-radius: 12px; text-align: center; color: white; box-shadow: 0 4px 8px rgba(0,0,0,0.15);}
    .status-card.voltage{background: linear-gradient(135deg, #667eea, #764ba2);}
    .status-card.current{background: linear-gradient(135deg, #f093fb, #f5576c);}
    .status-card.power{background: linear-gradient(135deg, #4facfe, #00f2fe);}
    .status-card.frequency{background: linear-gradient(135deg, #43e97b, #38f9d7);}
    .status-value{font-size: 2.5rem; font-weight: bold; margin-bottom: 4px;}
    .status-label{font-size: 0.9rem; opacity: 0.9;}

    .pv-section{margin: 20px 0;}
    .pv-grid{display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 12px;}
    .pv-card{background: linear-gradient(135deg, #ffecd2, #fcb69f); padding: 12px; border-radius: 10px; text-align: center; box-shadow: 0 2px 4px rgba(0,0,0,0.1);}
    .pv-title{font-weight: bold; margin-bottom: 8px; color: #8b4513;}
    .pv-values{font-size: 0.9rem; color: #d2691e;}

    .system-grid{display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 12px; margin: 16px 0;}
    .system-item{display: flex; justify-content: space-between; padding: 12px; background: linear-gradient(135deg, #e3ffe7, #d9e7ff); border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.05);}
    .system-label{font-weight: 500; color: #2d3748;}
    .system-value{font-weight: bold; color: #4a90e2;}

    .charts-section{margin: 20px 0;}
    .charts-grid{display: grid; grid-template-columns: repeat(auto-fit, minmax(350px, 1fr)); gap: 16px;}
    .chart-container{padding: 16px; background: #f8f9fa; border-radius: 8px; box-shadow: inset 0 1px 3px rgba(0,0,0,0.1); position: relative;}
    .chart-container canvas{width: 100%; height: auto;}

    details summary{padding: 8px 12px; background: #4a90e2; color: white; border-radius: 6px; margin-bottom: 8px;}
    details[open] summary{border-radius: 6px 6px 0 0;}

    /* Enhanced FOTA Section */
    .fota-card{background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white;}
    .fota-card h2{color: white; margin-bottom: 20px;}
    .file-upload-area{background: rgba(255,255,255,0.1); border: 2px dashed rgba(255,255,255,0.3); border-radius: 12px; padding: 20px; text-align: center; margin-bottom: 16px;}
    .file-info{color: rgba(255,255,255,0.8); margin-top: 8px; font-size: 14px;}
    .fota-settings{display: grid; grid-template-columns: 1fr 1fr; gap: 16px; margin-bottom: 16px;}
    .setting-group{display: flex; flex-direction: column;}
    .setting-group label{color: rgba(255,255,255,0.9); margin-bottom: 4px; font-weight: 500;}
    .setting-input{padding: 8px 12px; border-radius: 8px; border: none; background: rgba(255,255,255,0.9); color: #333;}
    .fota-actions{display: flex; align-items: center; gap: 12px; flex-wrap: wrap;}
    .primary-btn{background: linear-gradient(135deg, #4facfe, #00f2fe); color: white; border: none; padding: 10px 20px; border-radius: 8px; font-weight: 600; cursor: pointer; transition: all 0.3s;}
    .primary-btn:hover{transform: translateY(-2px); box-shadow: 0 4px 12px rgba(79, 172, 254, 0.4);}
    .secondary-btn{background: rgba(255,255,255,0.2); color: white; border: 1px solid rgba(255,255,255,0.3); padding: 10px 20px; border-radius: 8px; cursor: pointer; transition: all 0.3s;}
    .secondary-btn:hover{background: rgba(255,255,255,0.3);}
    .fota-progress{display: none; height: 6px; border-radius: 3px; background: rgba(255,255,255,0.2); overflow: hidden;}
    .log-container{margin-top: 16px; max-height: 250px; overflow-y: auto; background: rgba(0,0,0,0.2); border-radius: 8px; padding: 12px; font-family: 'Courier New', monospace; font-size: 12px; color: rgba(255,255,255,0.9);}
    
    /* Enhanced JSON Message Display */
    .json-message{background: rgba(255,255,255,0.1); border-left: 3px solid #4ade80; padding: 8px 12px; margin: 4px 0; border-radius: 4px; font-size: 11px; line-height: 1.4;}
    .json-content{color: #d1d5db; margin-top: 4px; white-space: pre-wrap; font-family: 'Courier New', monospace; font-size: 10px; max-height: 120px; overflow-y: auto; background: rgba(0,0,0,0.3); padding: 8px; border-radius: 4px;}
    .json-key{color: #60a5fa;} .json-string{color: #34d399;} .json-number{color: #fbbf24;} .json-bool{color: #f87171;}
    .message-timestamp{color: #9ca3af; font-size: 10px; float: right;}
    .message-topic{color: #c084fc; font-weight: bold; display: inline-block; margin-bottom: 4px;}
    .expand-btn{cursor: pointer; color: #60a5fa; font-size: 10px; margin-left: 8px; text-decoration: underline;}
    .expand-btn:hover{color: #93c5fd;}

    /* Enhanced Config Section */
    .config-card{background: linear-gradient(135deg, #43e97b 0%, #38f9d7 100%); color: white;}
    .config-card h2{color: white; margin-bottom: 20px;}
    .config-form{background: rgba(255,255,255,0.1); border-radius: 12px; padding: 20px;}
    .config-grid{display: grid; grid-template-columns: 1fr 1fr; gap: 16px; margin-bottom: 16px;}
    .config-field{display: flex; flex-direction: column;}
    .config-field label{color: rgba(255,255,255,0.9); margin-bottom: 4px; font-weight: 500; font-size: 14px;}
    .config-input{padding: 8px 12px; border-radius: 8px; border: none; background: rgba(255,255,255,0.9); color: #333;}
    .config-actions{display: flex; gap: 12px;}
    .config-btn{background: rgba(255,255,255,0.9); color: #43e97b; border: none; padding: 10px 16px; border-radius: 8px; font-weight: 600; cursor: pointer; transition: all 0.3s;}
    .config-btn:hover{background: white; transform: translateY(-1px);}

    /* Enhanced Write Section */
    .write-card{background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%); color: white;}
    .write-card h2{color: white; margin-bottom: 20px;}
    .write-form{background: rgba(255,255,255,0.1); border-radius: 12px; padding: 20px;}
    .write-fields{display: grid; grid-template-columns: 1fr 1fr auto; gap: 12px; align-items: end;}
    .write-field{display: flex; flex-direction: column;}
    .write-field label{color: rgba(255,255,255,0.9); margin-bottom: 4px; font-weight: 500;}
    .write-input{padding: 8px 12px; border-radius: 8px; border: none; background: rgba(255,255,255,0.9); color: #333;}
    .write-btn{background: rgba(255,255,255,0.9); color: #f5576c; border: none; padding: 10px 16px; border-radius: 8px; font-weight: 600; cursor: pointer; transition: all 0.3s; height: fit-content;}
    .write-btn:hover{background: white; transform: translateY(-1px);}
  </style>
</head>
<body>
  <h1>ESP32 Console — FOTA, Config, Write & Telemetry</h1>

  <div class="grid">
    <div class="card fota-card">
      <h2>Firmware Upload & Update</h2>
      <form id="uploadForm">
        <div class="file-upload-area">
          <input type="file" id="fw" accept=".bin" required />
          <div class="file-info">Select .bin firmware file</div>
        </div>
        <div class="fota-settings">
          <div class="setting-group">
            <label>Version</label>
            <input type="text" id="version" value="v1.0.0" class="setting-input"/>
          </div>
          <div class="setting-group">
            <label>Chunk Size</label>
            <input type="number" id="chunk" value="4096" min="512" step="512" class="setting-input"/>
          </div>
        </div>
        <div class="fota-actions">
          <button type="submit" class="primary-btn">Start FOTA</button>
          <progress id="prog" value="0" max="100" class="fota-progress"></progress>
          <button type="button" id="rebootBtn" class="secondary-btn">Send Reboot</button>
        </div>
      </form>
      <div style="margin-top: 16px; color: rgba(255,255,255,0.8); font-size: 13px;">FOTA publishes sealed <code>manifest → chunk(s) → finish</code> to <code>{{topic_cmd}}</code></div>
      <div id="fotaLog" class="log-container"></div>
    </div>

    <div class="card config-card">
      <h2>Device Configuration</h2>
      <div style="margin-bottom: 16px; color: rgba(255,255,255,0.8); font-size: 13px;">Sends a sealed JSON to <code>{{topic_config}}</code>. Only changed fields are updated in the form; the server merges and sends full config.</div>
      <form id="cfgForm" class="config-form">
        <div class="config-grid">
          <div class="config-field">
            <label>Poll Period (ms)</label>
            <input type="number" id="poll_period_ms" min="100" step="100" class="config-input" placeholder="10000"/>
          </div>
          <div class="config-field">
            <label>Upload Period (ms)</label>
            <input type="number" id="upload_period_ms" min="100" step="100" class="config-input" placeholder="20000"/>
          </div>
          <div class="config-field">
            <label>Buffer Capacity</label>
            <input type="number" id="buffer_capacity" min="1" step="1" class="config-input" placeholder="256"/>
          </div>
          <div class="config-field">
            <label>Register Request ID</label>
            <input type="number" id="reg_req_id_1" min="0" step="1" class="config-input" placeholder="1023"/>
          </div>
        </div>
        <div class="config-actions">
          <button type="submit" class="config-btn">Send Config</button>
          <button type="button" id="loadCfgBtn" class="config-btn">Load Current</button>
        </div>
      </form>
      <div id="cfgLog" class="log-container"></div>
    </div>
  </div>

  <div class="card write-card">
    <h2>Write Register</h2>
    <div style="margin-bottom: 16px; color: rgba(255,255,255,0.8); font-size: 13px;">Publishes a sealed JSON to <code>{{topic_write}}</code> with address & value.</div>
    <form id="writeForm" class="write-form">
      <div class="write-fields">
        <div class="write-field">
          <label>Register Address</label>
          <input type="number" id="wr_address" min="0" step="1" required class="write-input" placeholder="0"/>
        </div>
        <div class="write-field">
          <label>Register Value</label>
          <input type="number" id="wr_value" step="1" required class="write-input" placeholder="0"/>
        </div>
        <button type="submit" class="write-btn">Send Write</button>
      </div>
    </form>
    <div id="writeLog" class="log-container"></div>
  </div>

  <!-- Solar Data Dashboard -->
  <div class="card">
    <h2>Live Solar Data Dashboard</h2>
    <div class="muted">Real-time solar inverter telemetry from <code>{{topic_data}}</code></div>
    
    <!-- Status Overview -->
    <div class="status-grid">
      <div class="status-card voltage">
        <div class="status-value" id="currentVoltage">--</div>
        <div class="status-label">AC Voltage (V)</div>
      </div>
      <div class="status-card current">
        <div class="status-value" id="currentCurrent">--</div>
        <div class="status-label">AC Current (A)</div>
      </div>
      <div class="status-card power">
        <div class="status-value" id="currentPower">--</div>
        <div class="status-label">AC Power (W)</div>
      </div>
      <div class="status-card frequency">
        <div class="status-value" id="currentFreq">--</div>
        <div class="status-label">Frequency (Hz)</div>
      </div>
    </div>

    <!-- PV Panels Section -->
    <div class="pv-section">
      <h3>PV Panel Status</h3>
      <div class="pv-grid">
        <div class="pv-card">
          <div class="pv-title">PV1</div>
          <div class="pv-values">
            <span id="pv1Voltage">--</span> • <span id="pv1Current">--</span>
          </div>
        </div>
        <div class="pv-card">
          <div class="pv-title">PV2</div>
          <div class="pv-values">
            <span id="pv2Voltage">--</span> • <span id="pv2Current">--</span>
          </div>
        </div>
        <div class="pv-card">
          <div class="pv-title">PV3</div>
          <div class="pv-values">
            <span id="pv3Voltage">--</span> • <span id="pv3Current">--</span>
          </div>
        </div>
      </div>
    </div>

    <!-- System Status -->
    <div class="system-grid">
      <div class="system-item">
        <span class="system-label">Temperature:</span>
        <span id="sysTemp" class="system-value">--</span>
      </div>
      <div class="system-item">
        <span class="system-label">Export Ratio:</span>
        <span id="exportRatio" class="system-value">--</span>
      </div>
      <div class="system-item">
        <span class="system-label">Last Update:</span>
        <span id="lastUpdate" class="system-value">--:--:--</span>
      </div>
    </div>

    <!-- Live Charts -->
    <div class="charts-section">
      <h3>Live Charts - All 10 Registers</h3>
      <div class="charts-grid">
        <div class="chart-container">
          <canvas id="voltageChart" width="350" height="180"></canvas>
        </div>
        <div class="chart-container">
          <canvas id="currentChart" width="350" height="180"></canvas>
        </div>
        <div class="chart-container">
          <canvas id="frequencyChart" width="350" height="180"></canvas>
        </div>
        <div class="chart-container">
          <canvas id="powerChart" width="350" height="180"></canvas>
        </div>
        <div class="chart-container">
          <canvas id="pv1VChart" width="350" height="180"></canvas>
        </div>
        <div class="chart-container">
          <canvas id="pv1IChart" width="350" height="180"></canvas>
        </div>
        <div class="chart-container">
          <canvas id="pv2VChart" width="350" height="180"></canvas>
        </div>
        <div class="chart-container">
          <canvas id="pv2IChart" width="350" height="180"></canvas>
        </div>
        <div class="chart-container">
          <canvas id="pv3VChart" width="350" height="180"></canvas>
        </div>
        <div class="chart-container">
          <canvas id="temperatureChart" width="350" height="180"></canvas>
        </div>
      </div>
    </div>

    <!-- Raw Data Table -->
    <details style="margin-top: 20px;">
      <summary style="cursor: pointer; font-weight: bold;">Raw Data Records</summary>
      <table id="dataTable" style="margin-top:12px">
        <thead><tr><th>Timestamp</th><th>V(AC)</th><th>I(AC)</th><th>Freq</th><th>PV1-V</th><th>PV1-I</th><th>PV2-V</th><th>PV2-I</th><th>PV3-V</th><th>PV3-I</th><th>Temp</th></tr></thead>
        <tbody></tbody>
      </table>
      <div class="muted">Showing most recent 50 records.</div>
    </details>
  </div>

<script>
const logEl = document.getElementById('fotaLog');
const cfgLog = document.getElementById('cfgLog');
const writeLog = document.getElementById('writeLog');
// Enhanced JSON formatting functions
function formatJSON(obj) {
  return JSON.stringify(obj, null, 2)
    .replace(/"([^"]+)":/g, '<span class="json-key">"$1":</span>')
    .replace(/"([^"]+)"/g, '<span class="json-string">"$1"</span>')
    .replace(/\b(\d+\.?\d*)\b/g, '<span class="json-number">$1</span>')
    .replace(/\b(true|false|null)\b/g, '<span class="json-bool">$1</span>');
}

function createJSONMessage(topic, content, timestamp) {
  const container = document.createElement('div');
  container.className = 'json-message';
  
  const isJSON = typeof content === 'object';
  const shortContent = isJSON ? JSON.stringify(content).substring(0, 80) + '...' : content;
  
  container.innerHTML = `
    <div>
      <span class="message-topic">[${topic}]</span>
      <span class="message-timestamp">${new Date(timestamp).toLocaleTimeString()}</span>
    </div>
    <div class="message-preview">${shortContent}${isJSON ? '<span class="expand-btn" onclick="toggleJSON(this)">▼ expand</span>' : ''}</div>
    ${isJSON ? `<div class="json-content" style="display: none;">${formatJSON(content)}</div>` : ''}
  `;
  
  return container;
}

function toggleJSON(btn) {
  const content = btn.parentElement.nextElementSibling;
  if (content.style.display === 'none') {
    content.style.display = 'block';
    btn.textContent = '▲ collapse';
  } else {
    content.style.display = 'none';
    btn.textContent = '▼ expand';
  }
}

function log(line, obj = null){ 
  if (obj) {
    logEl.prepend(createJSONMessage('FOTA', obj, Date.now()));
  } else {
    const p = document.createElement('div'); 
    p.textContent = `[${new Date().toLocaleTimeString()}] ${line}`; 
    logEl.prepend(p); 
  }
}

function clog(line, obj = null){ 
  if (obj) {
    cfgLog.prepend(createJSONMessage('CONFIG', obj, Date.now()));
  } else {
    const p = document.createElement('div'); 
    p.textContent = `[${new Date().toLocaleTimeString()}] ${line}`; 
    cfgLog.prepend(p); 
  }
}

function wlog(line, obj = null){ 
  if (obj) {
    writeLog.prepend(createJSONMessage('WRITE', obj, Date.now()));
  } else {
    const p = document.createElement('div'); 
    p.textContent = `[${new Date().toLocaleTimeString()}] ${line}`; 
    writeLog.prepend(p); 
  }
}
function fmt(ts){ const d=new Date(ts); return d.toLocaleString(); }

async function pollEvents(){
  try{
    const r = await fetch('/api/fota/events'); const j = await r.json();
    logEl.innerHTML='';
    for (let i=j.events.length-1;i>=0;i--){
      const e = j.events[i];
      if (e.topic && typeof e === 'object' && Object.keys(e).length > 2) {
        // Display as formatted JSON message
        logEl.prepend(createJSONMessage(e.topic.toUpperCase(), e, e.ts));
      } else {
        // Display as simple text  
        log(`[${fmt(e.ts)}] ${e.topic || 'EVENT'}: ${e.info || JSON.stringify(e)}`);
      }
    }
  }catch{}
}
// Chart data storage for all 10 registers
let chartData = {
  voltage: [],
  current: [],
  frequency: [],
  power: [],
  pv1V: [],
  pv1I: [],
  pv2V: [],
  pv2I: [],
  pv3V: [],
  temperature: []
};
let timeLabels = [];



async function pollData(){
  try{
    const r = await fetch('/api/data?limit=50'); const j = await r.json();
    const records = j.records || [];
    
    if (records.length > 0) {
      const latest = records[records.length - 1];
      const regs = latest.registers || {};
      
      // Update current values display
      const voltage = (regs[0]?.value || 0) * 0.1; // Scale to volts
      const current = (regs[1]?.value || 0) * 0.01; // Scale to amps  
      const frequency = (regs[2]?.value || 0) * 0.01; // Scale to Hz
      const pv1_voltage = (regs[3]?.value || 0) * 0.1;
      const pv1_current = (regs[4]?.value || 0) * 0.01;
      const pv2_voltage = (regs[5]?.value || 0) * 0.1;
      const pv2_current = (regs[6]?.value || 0) * 0.01;
      const pv3_voltage = (regs[7]?.value || 0) * 0.1;
      const pv3_current = (regs[8]?.value || 0) * 0.01;
      const temperature = (regs[9]?.value || 0) * 0.1;
      const power = voltage * current;

      // Update status cards
      document.getElementById('currentVoltage').textContent = voltage.toFixed(1);
      document.getElementById('currentCurrent').textContent = current.toFixed(2);
      document.getElementById('currentPower').textContent = Math.round(power);
      document.getElementById('currentFreq').textContent = frequency.toFixed(1);

      // Update PV panels
      document.getElementById('pv1Voltage').textContent = pv1_voltage.toFixed(1) + 'V';
      document.getElementById('pv1Current').textContent = pv1_current.toFixed(2) + 'A';
      document.getElementById('pv2Voltage').textContent = pv2_voltage.toFixed(1) + 'V';
      document.getElementById('pv2Current').textContent = pv2_current.toFixed(2) + 'A';
      document.getElementById('pv3Voltage').textContent = pv3_voltage.toFixed(1) + 'V';
      document.getElementById('pv3Current').textContent = pv3_current.toFixed(2) + 'A';

      // Update system status
      document.getElementById('sysTemp').textContent = temperature.toFixed(1) + '°C';
      const exportRatio = power > 0 ? Math.min(100, (power / 2000 * 100)) : 0;
      document.getElementById('exportRatio').textContent = exportRatio.toFixed(0) + '%';
      document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();

      // Update all chart data with real values
      const now = new Date().toLocaleTimeString();
      chartData.voltage.push(voltage);
      chartData.current.push(current);
      chartData.frequency.push(frequency);
      chartData.power.push(power);
      chartData.pv1V.push(pv1_voltage);
      chartData.pv1I.push(pv1_current);
      chartData.pv2V.push(pv2_voltage);
      chartData.pv2I.push(pv2_current);
      chartData.pv3V.push(pv3_voltage);
      chartData.temperature.push(temperature);
      timeLabels.push(now);
      
      // Keep only last 20 points for all charts
      if (timeLabels.length > 20) {
        Object.keys(chartData).forEach(key => chartData[key].shift());
        timeLabels.shift();
      }
      
      updateAllCharts();
    }

    // Update raw data table
    const tb = document.querySelector('#dataTable tbody'); tb.innerHTML='';
    records.slice(-50).reverse().forEach(rec=>{
      const tr = document.createElement('tr');
      const regs = rec.registers || {};
      
      const cells = [
        new Date(rec.timestamp).toLocaleString(),
        ((regs[0]?.value || 0) * 0.1).toFixed(1), // AC Voltage
        ((regs[1]?.value || 0) * 0.01).toFixed(2), // AC Current
        ((regs[2]?.value || 0) * 0.01).toFixed(1), // Frequency
        ((regs[3]?.value || 0) * 0.1).toFixed(1), // PV1 Voltage
        ((regs[4]?.value || 0) * 0.01).toFixed(2), // PV1 Current
        ((regs[5]?.value || 0) * 0.1).toFixed(1), // PV2 Voltage
        ((regs[6]?.value || 0) * 0.01).toFixed(2), // PV2 Current
        ((regs[7]?.value || 0) * 0.1).toFixed(1), // PV3 Voltage
        ((regs[8]?.value || 0) * 0.01).toFixed(2), // PV3 Current
        ((regs[9]?.value || 0) * 0.1).toFixed(1) // Temperature
      ];
      
      cells.forEach(cellData => {
        const td = document.createElement('td');
        td.textContent = cellData;
        tr.appendChild(td);
      });
      
      tb.appendChild(tr);
    });
  }catch(e){console.error('Poll data error:', e);}
}

// Update all 10 charts
function updateAllCharts() {
  drawChart('voltageChart', chartData.voltage, timeLabels, 'AC Voltage (V)', '#667eea');
  drawChart('currentChart', chartData.current, timeLabels, 'AC Current (A)', '#f093fb');
  drawChart('frequencyChart', chartData.frequency, timeLabels, 'Grid Frequency (Hz)', '#43e97b');
  drawChart('powerChart', chartData.power, timeLabels, 'AC Power (W)', '#4facfe');
  drawChart('pv1VChart', chartData.pv1V, timeLabels, 'PV1 Voltage (V)', '#ff9500');
  drawChart('pv1IChart', chartData.pv1I, timeLabels, 'PV1 Current (A)', '#ff6b35');
  drawChart('pv2VChart', chartData.pv2V, timeLabels, 'PV2 Voltage (V)', '#ffa726');
  drawChart('pv2IChart', chartData.pv2I, timeLabels, 'PV2 Current (A)', '#ff7043');
  drawChart('pv3VChart', chartData.pv3V, timeLabels, 'PV3 Voltage (V)', '#ffb74d');
  drawChart('temperatureChart', chartData.temperature, timeLabels, 'Temperature (°C)', '#e53e3e');
}

function drawChart(canvasId, data, labels, title, color) {
  const canvas = document.getElementById(canvasId);
  if (!canvas) return;
  
  const ctx = canvas.getContext('2d');
  const width = canvas.width;
  const height = canvas.height;
  
  // Clear canvas
  ctx.clearRect(0, 0, width, height);
  
  // Draw background with gradient
  const gradient = ctx.createLinearGradient(0, 0, 0, height);
  gradient.addColorStop(0, '#ffffff');
  gradient.addColorStop(1, '#f8f9fa');
  ctx.fillStyle = gradient;
  ctx.fillRect(0, 0, width, height);
  
  // Draw border
  ctx.strokeStyle = '#e9ecef';
  ctx.lineWidth = 1;
  ctx.strokeRect(0, 0, width, height);
  
  if (data.length < 1) {
    // No data message
    ctx.fillStyle = '#6c757d';
    ctx.font = '14px sans-serif';
    ctx.textAlign = 'center';
    ctx.fillText('No Data Available', width / 2, height / 2);
    ctx.fillText(title, width / 2, 25);
    return;
  }
  
  // Find min/max for scaling
  const minVal = Math.min(...data) * 0.98;
  const maxVal = Math.max(...data) * 1.02;
  const range = maxVal - minVal || 1;
  
  // Draw title with better styling
  ctx.fillStyle = '#212529';
  ctx.font = 'bold 14px sans-serif';
  ctx.textAlign = 'center';
  ctx.fillText(title, width / 2, 18);
  
  // Draw grid lines
  ctx.strokeStyle = '#dee2e6';
  ctx.lineWidth = 0.5;
  for (let i = 1; i <= 4; i++) {
    const y = 35 + (height - 65) * i / 5;
    ctx.beginPath();
    ctx.moveTo(45, y);
    ctx.lineTo(width - 15, y);
    ctx.stroke();
  }
  
  // Draw Y-axis
  ctx.strokeStyle = '#495057';
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(45, 30);
  ctx.lineTo(45, height - 25);
  ctx.stroke();
  
  // Draw X-axis
  ctx.beginPath();
  ctx.moveTo(45, height - 25);
  ctx.lineTo(width - 15, height - 25);
  ctx.stroke();
  
  if (data.length < 2) return;
  
  // Draw area under curve
  ctx.fillStyle = color + '20'; // Add transparency
  ctx.beginPath();
  ctx.moveTo(45, height - 25);
  data.forEach((value, index) => {
    const x = 45 + (width - 60) * index / (data.length - 1);
    const y = height - 25 - ((value - minVal) / range) * (height - 60);
    ctx.lineTo(x, y);
  });
  ctx.lineTo(45 + (width - 60), height - 25);
  ctx.closePath();
  ctx.fill();
  
  // Draw data line
  ctx.strokeStyle = color;
  ctx.lineWidth = 2.5;
  ctx.beginPath();
  
  data.forEach((value, index) => {
    const x = 45 + (width - 60) * index / (data.length - 1);
    const y = height - 25 - ((value - minVal) / range) * (height - 60);
    
    if (index === 0) {
      ctx.moveTo(x, y);
    } else {
      ctx.lineTo(x, y);
    }
  });
  
  ctx.stroke();
  
  // Draw points
  ctx.fillStyle = color;
  data.forEach((value, index) => {
    const x = 45 + (width - 60) * index / (data.length - 1);
    const y = height - 25 - ((value - minVal) / range) * (height - 60);
    ctx.beginPath();
    ctx.arc(x, y, 2.5, 0, 2 * Math.PI);
    ctx.fill();
  });
  
  // Draw Y-axis labels
  ctx.fillStyle = '#6c757d';
  ctx.font = '10px sans-serif';
  ctx.textAlign = 'right';
  for (let i = 0; i <= 4; i++) {
    const value = minVal + (range * (4 - i) / 4);
    const y = 35 + (height - 65) * i / 5;
    ctx.fillText(value.toFixed(1), 40, y + 3);
  }
  
  // Draw current value box
  if (data.length > 0) {
    const currentVal = data[data.length - 1];
    ctx.fillStyle = color;
    ctx.fillRect(width - 65, 3, 60, 20);
    ctx.fillStyle = 'white';
    ctx.font = 'bold 11px sans-serif';
    ctx.textAlign = 'center';
    ctx.fillText(currentVal.toFixed(1), width - 35, 16);
  }
  
  // Draw time labels (first and last)
  if (labels.length >= 2) {
    ctx.fillStyle = '#6c757d';
    ctx.font = '9px sans-serif';
    ctx.textAlign = 'left';
    const firstTime = labels[0].split(':').slice(1, 3).join(':'); // Remove seconds for space
    ctx.fillText(firstTime, 47, height - 10);
    ctx.textAlign = 'right';
    const lastTime = labels[labels.length - 1].split(':').slice(1, 3).join(':');
    ctx.fillText(lastTime, width - 17, height - 10);
  }
}

setInterval(pollEvents, 1500);
setInterval(pollData, 1500);
pollEvents(); pollData();

// Initialize empty charts
updateAllCharts();

document.getElementById('rebootBtn').onclick = async ()=>{
  const r = await fetch('/api/reboot', {method:'POST'}); const j = await r.json();
  log('reboot sent:', j);
};

document.getElementById('uploadForm').onsubmit = async (e)=>{
  e.preventDefault();
  const fw = document.getElementById('fw').files[0];
  const version = document.getElementById('version').value || 'v1.0.0';
  const chunk = parseInt(document.getElementById('chunk').value || '4096',10);
  if (!fw){ alert('Pick a .bin'); return; }
  const fd = new FormData(); fd.append('firmware', fw); fd.append('version', version); fd.append('chunk', chunk);
  const prog = document.getElementById('prog'); prog.style.display='inline-block'; prog.value=0;
  log('Uploading firmware and starting FOTA...');
  const r = await fetch('/api/fota/upload', {method:'POST', body: fd});
  const j = await r.json(); log('FOTA started:', j); setTimeout(()=>prog.value=20, 300);
};

// ---- Config UI ----
async function loadConfig(){
  const r = await fetch('/api/config'); const j = await r.json();
  for (const k of ['poll_period_ms','upload_period_ms','buffer_capacity','reg_req_id_1']){
    if (j.config[k] !== undefined) document.getElementById(k).value = j.config[k];
  }
  clog('config loaded:', j.config);
}
document.getElementById('loadCfgBtn').onclick = loadConfig;

document.getElementById('cfgForm').onsubmit = async (e)=>{
  e.preventDefault();
  const body = {};
  for (const k of ['poll_period_ms','upload_period_ms','buffer_capacity','reg_req_id_1']){
    const v = document.getElementById(k).value;
    if (v !== '') body[k] = Number(v);
  }
  const r = await fetch('/api/config', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify(body)});
  const j = await r.json();
  clog('config sent:', j);
};

// ---- Write Register UI ----
document.getElementById('writeForm').onsubmit = async (e)=>{
  e.preventDefault();
  const address = Number(document.getElementById('wr_address').value);
  const value   = Number(document.getElementById('wr_value').value);
  if (Number.isNaN(address) || Number.isNaN(value)) { alert('address/value must be numbers'); return; }
  const r = await fetch('/api/write', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({address, value})});
  const j = await r.json();
  wlog('register write:', j);
};
</script>
</body>
</html>
"""

@app.route("/")
def index():
    return (INDEX_HTML
            .replace("{{topic_cmd}}", TOPIC_CMD)
            .replace("{{topic_config}}", TOPIC_CONFIG)
            .replace("{{topic_data}}", TOPIC_DATA)
            .replace("{{topic_write}}", TOPIC_WRITE))

# ---- Events/Data/Status ----
@app.route("/api/fota/events")
def api_fota_events():
    return jsonify({"events": fota_events})

@app.route("/api/data")
def api_data():
    limit = int(request.args.get("limit", "200"))
    return jsonify({"records": data_records[-limit:]})

@app.route("/api/status")
def api_status():
    return jsonify({
        "device": DEV_ID,
        "broker": f"{BROKER_HOST}:{BROKER_PORT}",
        "topics": {"cmd": TOPIC_CMD, "status": TOPIC_STAT, "data": TOPIC_DATA, "config": TOPIC_CONFIG, "ack": TOPIC_ACK, "write": TOPIC_WRITE},
        "fota_active": state["active"],
        "next_offset": state["next_offset"],
        "total": state["total"]
    })

# ---- Reboot ----
@app.route("/api/reboot", methods=["POST"])
def api_reboot():
    mqttc.publish(TOPIC_CMD, seal_downlink({"op": "reboot"}, msg_type=2), qos=1, retain=False)
    push_event({"topic": "fota/status", "ev": "reboot_sent"})
    return jsonify({"ok": True})

# ---- FOTA upload ----
@app.route("/api/fota/upload", methods=["POST"])
def api_fota_upload():
    if "firmware" not in request.files:
        return jsonify({"ok": False, "error": "no_firmware"}), 400
    f = request.files["firmware"]
    version = request.form.get("version", "v1.0.0")
    try: chunk = int(request.form.get("chunk", str(DEFAULT_CHUNK)))
    except: chunk = DEFAULT_CHUNK

    fd, path = tempfile.mkstemp(prefix="fw_", suffix=".bin"); os.close(fd)
    f.save(path)
    with open(path, "rb") as fh: fw = fh.read()
    os.remove(path)

    sha_hex = hashlib.sha256(fw).hexdigest().upper()
    nonce = os.urandom(16)
    state.update({"firmware": fw, "version": version, "chunk": chunk, "sha_hex": sha_hex,
                  "nonce": nonce, "next_offset": 0, "total": len(fw), "active": True})
    manifest = {"op": "manifest", "version": version, "size": len(fw), "chunk": chunk,
                "sha256_hex": sha_hex, "nonce_b64": base64.b64encode(nonce).decode("ascii")}
    mqttc.publish(TOPIC_CMD, seal_downlink(manifest, msg_type=2), qos=1, retain=False)
    push_event({"topic": "fota/status", "ev": "manifest_sent", "size": len(fw), "sha256_hex": sha_hex, "chunk": chunk})
    return jsonify({"ok": True, "version": version, "size": len(fw), "sha256_hex": sha_hex, "chunk": chunk})

# ---- CONFIG: get + merge + send ----
@app.route("/api/config", methods=["GET", "POST"])
def api_config():
    global current_config
    if request.method == "GET":
        return jsonify({"config": current_config})
    try:
        patch = request.get_json(force=True) or {}
    except Exception:
        return jsonify({"ok": False, "error": "bad_json"}), 400
    allowed = {"poll_period_ms", "upload_period_ms", "buffer_capacity", "reg_req_id_1"}
    for k,v in patch.items():
        if k in allowed and isinstance(v, (int, float)):
            current_config[k] = int(v)
    publish_config(current_config.copy())
    push_event({"topic":"config", "sent": current_config.copy()})
    return jsonify({"ok": True, "sent": current_config})

# ---- WRITE: address/value -> publish sealed JSON ----
def make_write_payload(address: int, value: int) -> Dict[str, Any]:
    """
    Adjust this if your device expects a different shape.
    Current shape:
      { "op": "write", "address": <int>, "value": <int> }
    """
    return {"op": "write", "address": int(address), "value": int(value)}

@app.route("/api/write", methods=["POST"])
def api_write():
    try:
        body = request.get_json(force=True) or {}
    except Exception:
        return jsonify({"ok": False, "error": "bad_json"}), 400
    if "address" not in body or "value" not in body:
        return jsonify({"ok": False, "error": "need_address_and_value"}), 400
    try:
        addr = int(body["address"]); val = int(body["value"])
    except Exception:
        return jsonify({"ok": False, "error": "address_value_must_be_int"}), 400

    payload = make_write_payload(addr, val)
    publish_write(payload)
    push_event({"topic":"write", "sent": payload})
    return jsonify({"ok": True, "sent": payload})

# ---------- favicon ----------
@app.route("/favicon.ico")
def favicon():
    return ("", 204)

if __name__ == "__main__":
    time.sleep(0.5)
    app.run(host="0.0.0.0", port=8080, debug=False)
