#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os, time, json, hmac, base64, hashlib, struct, threading, tempfile
from typing import Dict, Any, List, Tuple, Optional
from flask import Flask, request, jsonify
import paho.mqtt.client as mqtt
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

# =================== CONFIG ===================
BROKER_HOST = "broker.emqx.io"      # or "localhost"
BROKER_PORT = 1883
MQTT_USER   = None                   # set if needed
MQTT_PASS   = None

DEV_ID      = "esp32-01"             # MUST match device's sec.begin(...)
TOPIC_CMD   = f"devices/{DEV_ID}/fota/cmd"          # downlink (server -> device, FOTA)
TOPIC_STAT  = f"devices/{DEV_ID}/fota/status"       # uplink status (device -> server)
TOPIC_DATA  = f"devices/{DEV_ID}/data/dulmin"       # uplink telemetry (device -> server)
TOPIC_CONFIG= f"devices/{DEV_ID}/config"            # downlink config (server -> device)
TOPIC_WRITE = f"devices/{DEV_ID}/write"             # downlink write (server -> device)

# NEW: dedicated ACK topics by function (uplink)
TOPIC_ACK_FOTA   = f"devices/{DEV_ID}/fota/status"
TOPIC_ACK_CONFIG = f"devices/{DEV_ID}/ack/config"
TOPIC_ACK_WRITE  = f"devices/{DEV_ID}/ack/write"

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

def try_open_uplink(sealed_b64: bytes) -> Optional[Dict[str, Any]]:
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

def try_open_ack(
    sealed_b64: bytes,
    device_id: str = None,
    psk_hex: str = None
) -> Optional[Tuple[Dict[str, Any], Dict[str, Any]]]:
    """
    Returns (obj, meta) on success, or None on failure.
    - obj: parsed JSON dict (your ack JSON)
    - meta: {} for plaintext case; for sealed: {'ver','type','boot','seq'}
    """
    device_id = device_id or DEV_ID
    psk_hex = psk_hex or PSK_HEX

    # 1) Try Base64 decode first
    try:
        raw = base64.b64decode(sealed_b64, validate=True)
    except Exception:
        # Not Base64 → maybe plaintext JSON
        try:
            obj = json.loads(sealed_b64.decode("utf-8"))
            return obj, {}   # plaintext path
        except Exception:
            return None

    # 2) If the decoded bytes *are* plaintext JSON (device sent unsealed)
    if raw and raw[0] in (0x7B, 0x5B):  # '{' or '['
        try:
            obj = json.loads(raw.decode("utf-8"))
            return obj, {}
        except Exception:
            return None

    # 3) Must be a sealed frame: parse header / HMAC / decrypt / JSON
    if len(raw) < HDR_LEN + MAC_LEN: return None

    hdr = raw[:HDR_LEN]
    ct  = raw[HDR_LEN:-MAC_LEN]
    mac = raw[-MAC_LEN:]

    # Derive keys for this device
    Kenc, Kmac = derive_keys(device_id, psk_hex)

    # Verify HMAC over (hdr || ct)
    tag = hmac.new(Kmac, hdr + ct, hashlib.sha256).digest()
    if not hmac.compare_digest(tag, mac):
        return None

    # Extract meta from header (layout: 1 ver, 1 type, 2 pad?, 8 boot, 8 seq, 12 iv)
    ver  = hdr[0]
    typ  = hdr[1]
    # little-endian u64 for boot/seq
    boot = int.from_bytes(hdr[4:12], "little", signed=False)
    seq  = int.from_bytes(hdr[12:20], "little", signed=False)
    iv12 = hdr[-12:]

    # Decrypt (CTR)
    plain = aes_ctr_crypt(Kenc, iv12, ct)

    # Parse JSON
    try:
        obj = json.loads(plain.decode("utf-8"))
    except Exception:
        return None

    meta = {"ver": ver, "type": typ, "boot": str(boot), "seq": str(seq)}
    return obj, meta

# ---------- Delta decompressor (supports mask+delta v1; tolerant to trailing) ----------

# ---------- Delta decompressor (supports mask+delta v1; tolerant to trailing) ----------

def get16_le(buf: bytes, i: int):
    if i + 2 > len(buf): return None, i
    return (buf[i] | (buf[i+1] << 8)) & 0xFFFF, i + 2

def get32_le(buf: bytes, i: int):
    if i + 4 > len(buf): return None, i
    return (buf[i] | (buf[i+1] << 8) | (buf[i+2] << 16) | (buf[i+3] << 24)) & 0xFFFFFFFF, i + 4

def get64_le(buf: bytes, i: int):
    if i + 8 > len(buf): return None, i
    v = 0
    for s in range(8):
        v |= (buf[i+s] << (8*s))
    return v & 0xFFFFFFFFFFFFFFFF, i + 8

def _get_varuint32(buf: bytes, i: int):
    """Returns (value, new_index) or (None, i) on error."""
    v = 0
    shift = 0
    while i < len(buf):
        b = buf[i]; i += 1
        v |= (b & 0x7F) << shift
        if (b & 0x80) == 0:
            return v, i
        shift += 7
        if shift > 28:  # guard
            return None, i
    return None, i

def _unzigzag32(u: int) -> int:
    return (u >> 1) ^ -(u & 1)

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

def _popcount16(x: int) -> int:
    return bin(x & 0xFFFF).count("1")

def _try_decompress_mask_delta(buf: bytes):
    i = 0
    base_ts, i = get64_le(buf, i)
    if base_ts is None: return None
    nrecs, i = get16_le(buf, i)
    if nrecs is None: return None

    last = [0] * 10
    ts = base_ts
    out = []

    for _ in range(nrecs):
        dt, i = _get_varuint32(buf, i)
        if dt is None: return None
        mask, i = get16_le(buf, i)
        if dt is None or mask is None: return None
        mask &= 0x03FF
        ts = (ts + dt) & 0xFFFFFFFFFFFFFFFF

        regs: Dict[str, Any] = {}
        setbits = _popcount16(mask)
        # quick sanity: impossible to have more than 10 regs or zero when dt>0 repeatedly
        if setbits > 10: return None

        for reg in range(10):
            if (mask & (1 << reg)) == 0:
                continue
            enc, i = _get_varuint32(buf, i)
            if enc is None: return None
            delta = _unzigzag32(enc)
            cur = int(last[reg]) + int(delta)
            if cur < 0: cur = 0
            if cur > 0xFFFF: cur = 0xFFFF
            last[reg] = cur

            d = decode_register(reg, cur)
            regs[d["name"]] = {
                "address": d["address"],
                "raw_value": d["raw_value"],
                "value": d["value"],
                "unit": d["unit"],
            }

        out.append({"timestamp": ts, "register_count": setbits, "registers": regs})

    # IMPORTANT: be lenient — ignore trailing bytes instead of failing
    return out

def _try_decompress_legacy(buf: bytes):
    """Legacy format: [base_ts:u64][nrecs:u16] then per rec [dt:u32][qty:u16][(addr:u16)(data:u16)]*qty"""
    i = 0
    base_ts, i = get64_le(buf, i)
    if base_ts is None: return None
    nrecs, i = get16_le(buf, i)
    if nrecs is None: return None

    prev_ts = base_ts
    out = []

    for _ in range(nrecs):
        dt_ms, i = get32_le(buf, i)
        qty,   i = get16_le(buf, i)
        if dt_ms is None or qty is None: return None

        need = qty * 4
        if i + need > len(buf): return None

        ts_ms = (prev_ts + dt_ms) & 0xFFFFFFFFFFFFFFFF
        regs: Dict[str, Any] = {}

        for __ in range(qty):
            addr, i2 = get16_le(buf, i)
            val,  i3 = get16_le(buf, i + 2)
            if addr is None or val is None: return None
            i = i + 4
            d = decode_register(addr, val)
            regs[d["name"]] = {"address": d["address"], "raw_value": d["raw_value"], "value": d["value"], "unit": d["unit"]}

        out.append({"timestamp": ts_ms, "register_count": qty, "registers": regs})
        prev_ts = ts_ms

    return out

def decompress_delta(hex_string: str):
    try:
        buf = bytes.fromhex(hex_string)
    except Exception:
        return None

    # Try new mask+delta first
    out = _try_decompress_mask_delta(buf)
    if out is not None:
        return out

    # Fallback to legacy layout
    out = _try_decompress_legacy(buf)
    return out

# ---------- State & Loggers ----------
# Separate logs per function:
logs_data:  List[Dict[str, Any]] = []   # receive-only logs for DATA (top of page)
logs_fota:  List[Dict[str, Any]] = []   # send/recv/acks for FOTA block
logs_conf:  List[Dict[str, Any]] = []   # send/recv/acks for CONFIG block
logs_write: List[Dict[str, Any]] = []   # send/recv/acks for WRITE block
data_records: List[Dict[str, Any]] = [] # decoded telemetry records for charts/table

def _push_log(bucket: List[Dict[str, Any]], ev: Dict[str, Any]):
    bucket.append({"ts": int(time.time()*1000), **ev})
    if len(bucket) > MAX_EVENTS:
        del bucket[:len(bucket)-MAX_EVENTS]

current_config = {  # server-side cache; update as you like
    "poll_period_ms": 10000,
    "upload_period_ms": 20000,
    "buffer_capacity": 256,
    "reg_req_id_1": 1023  # Bitwise register selection ID (all 10 registers by default)
}

# ---------- MQTT ----------
mqttc = mqtt.Client()
if MQTT_USER and MQTT_PASS:
    mqttc.username_pw_set(MQTT_USER, MQTT_PASS)

def on_connect(client, userdata, flags, rc):
    client.subscribe([
        (TOPIC_STAT, 1),          # FOTA status (uplink)
        (TOPIC_DATA, 1),          # Telemetry (uplink)
        (TOPIC_ACK_FOTA, 1),      # ACKs for FOTA
        (TOPIC_ACK_CONFIG, 1),    # ACKs for CONFIG
        (TOPIC_ACK_WRITE, 1),     # ACKs for WRITE
    ])
    print(f"[MQTT] rc={rc}; subs: {TOPIC_STAT}, {TOPIC_DATA}, {TOPIC_ACK_FOTA}, {TOPIC_ACK_CONFIG}, {TOPIC_ACK_WRITE}")

def on_message(client, userdata, msg):
    topic = msg.topic

    # --- FOTA STATUS (uplink) ---
    if topic == TOPIC_STAT:
        evt = try_open_uplink(msg.payload)
        if not evt:
            _push_log(logs_fota, {"topic":"fota/status", "error":"parse_failed"})
            return
        _push_log(logs_fota, {"topic":"fota/status", **evt})
        ev = evt.get("ev")
        if ev == "need_chunks":
            state["next_offset"] = int(evt.get("next_offset", 0))
            state["total"]       = int(evt.get("total", 0))
            _send_next_chunk()
        elif ev == "progress":
            state["next_offset"] = int(evt.get("next_offset", 0))
            _send_next_chunk()

    # --- DATA (uplink) ---
    elif topic == TOPIC_DATA:
        obj = try_open_uplink(msg.payload)
        if not obj:
            _push_log(logs_data, {"topic":"data", "error":"parse_failed"})
            return
        # top logs (rx)
        _push_log(logs_data, {"topic":"data", "info":"rx", "preview": obj})

        if "payload_hex" in obj:
            recs = decompress_delta(obj["payload_hex"])
            if recs is not None:
                # Fix timestamps - add server timestamp as backup
                current_time = int(time.time() * 1000)
                for i, rec in enumerate(recs):
                    if rec["timestamp"] < 946684800000:  # Before year 2000
                        rec["server_timestamp"] = current_time - (len(recs) - i) * 10000
                    else:
                        rec["server_timestamp"] = rec["timestamp"]
                data_records.extend(recs)
                if len(data_records) > MAX_RECORDS:
                    del data_records[:len(data_records)-MAX_RECORDS]
                _push_log(logs_data, {"topic":"data", "info": f"decoded {len(recs)} records"})
            else:
                _push_log(logs_data, {"topic":"data", "info":"delta_decompress_failed"})
        else:
            # Non-delta payloads still logged
            pass

    # --- ACKS (uplink) ---
    elif topic == TOPIC_ACK_FOTA:
        try: obj, meta = try_open_ack(msg.payload)
        except: obj, meta = None, {}
        _push_log(logs_fota, {"topic":"ack/fota", "ack": obj, "meta": meta})

    elif topic == TOPIC_ACK_CONFIG:
        try: obj, meta = try_open_ack(msg.payload)
        except: obj, meta = None, {}
        _push_log(logs_conf, {"topic":"ack/config", "ack": obj, "meta": meta})

    elif topic == TOPIC_ACK_WRITE:
        
        res = try_open_ack(msg.payload)
        if not res:
            _push_log(logs_write, {"topic":"ack/write", "error":"parse_failed"})
        else:
            obj, meta = res
            _push_log(logs_write, {"topic":"ack/write", "ack": obj, "meta": meta})


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
    payload = seal_downlink(obj, msg_type=msg_type)
    mqttc.publish(TOPIC_CMD, payload, qos=1, retain=False)
    _push_log(logs_fota, {"topic":"fota/cmd", "dir":"tx", "payload": obj})

def publish_config(obj: Dict[str, Any]):
    payload = seal_downlink(obj, msg_type=2)
    mqttc.publish(TOPIC_CONFIG, payload, qos=1, retain=False)
    _push_log(logs_conf, {"topic":"config", "dir":"tx", "payload": obj})

def publish_write(obj: Dict[str, Any]):
    payload = seal_downlink(obj, msg_type=2)
    mqttc.publish(TOPIC_WRITE, payload, qos=1, retain=False)
    _push_log(logs_write, {"topic":"write", "dir":"tx", "payload": obj})

def _send_next_chunk():
    if not state["active"]:
        return
    off = state["next_offset"]; total = state["total"]; fw = state["firmware"]; chunk = state["chunk"]
    if off >= total:
        publish_cmd({"op": "finish"})
        _push_log(logs_fota, {"topic":"fota/cmd", "dir":"tx", "info":"finish_sent"})
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

INDEX_HTML ="""
<!doctype html>
<html>
<head>
  <meta charset="utf-8"/>
  <title>ESP32 Console — FOTA, Config, Write & Telemetry</title>
  <meta name="viewport" content="width=device-width,initial-scale=1"/>
  <style>
    body{font-family: ui-sans-serif,system-ui,-apple-system,Segoe UI; margin: 2rem; color:#334155; background: #f1f5f9; min-height: 100vh;}
    h1{margin:0 0 1.5rem 0; color: #1e40af; font-weight: 600; font-size: 1.875rem;}
    .grid{display:grid; grid-template-columns: 1fr 1fr; gap:20px}
    @media(max-width: 1200px){ .grid{grid-template-columns: 1fr} }
    @media(max-width: 768px){ 
      .fota-settings, .config-grid{grid-template-columns: 1fr;}
      .write-fields{grid-template-columns: 1fr; gap: 12px;}
      .status-grid{grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));}
      .charts-grid{grid-template-columns: 1fr;}
    }
    .card{border:1px solid #cbd5e1; border-radius:8px; padding:24px; margin-bottom:20px; box-shadow:0 2px 4px rgba(30,64,175,0.08); background: white;}
    button{padding:10px 16px; border-radius:6px; border:1px solid #cbd5e1; background:#f8fafc; cursor:pointer; transition: all 0.2s; font-weight: 500; color: #475569;}
    button:hover{background:#e2e8f0; border-color:#94a3b8;}
    table{border-collapse: collapse; width:100%;}
    th,td{border-bottom:1px solid #e2e8f0; padding:10px; text-align:left; vertical-align:top}
    .muted{color:#64748b; font-size:13px}
    input[type=file], input[type=text], input[type=number]{padding:10px 12px; border-radius: 6px; border: 1px solid #cbd5e1; background: white; color: #334155;}
    input[type=file]:focus, input[type=text]:focus, input[type=number]:focus{outline: none; border-color: #3b82f6; box-shadow: 0 0 0 3px rgba(59,130,246,0.1);}
    progress{width: 220px;}
    code{background:#e0e7ff; padding:3px 6px; border-radius:4px; color: #3730a3; font-size: 0.9em;}
    .kv{display:flex; gap:10px; flex-wrap:wrap; align-items:center}
    .kv label{display:flex; align-items:center; gap:6px}
    .stack{display:flex; flex-direction:column; gap:8px}

    /* Solar Dashboard Styles - Professional Blue Theme */
    .status-grid{display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 16px; margin: 20px 0;}
    .status-card{padding: 20px; border-radius: 8px; text-align: center; background: linear-gradient(135deg, #eff6ff 0%, #dbeafe 100%); border: 2px solid #bfdbfe;}
    .status-value{font-size: 2.5rem; font-weight: 600; margin-bottom: 8px; color: #1e40af;}
    .status-label{font-size: 0.875rem; color: #64748b; text-transform: uppercase; letter-spacing: 0.05em; font-weight: 500;}

    .pv-section{margin: 24px 0;}
    .pv-grid{display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 16px;}
    .pv-card{background: linear-gradient(135deg, #f8fafc 0%, #f1f5f9 100%); padding: 16px; border-radius: 8px; text-align: center; border: 1px solid #cbd5e1;}
    .pv-title{font-weight: 600; margin-bottom: 10px; color: #475569; font-size: 0.875rem;}
    .pv-values{font-size: 1.125rem; color: #1e40af; font-weight: 500;}

    .system-grid{display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 16px; margin: 20px 0;}
    .system-item{display: flex; justify-content: space-between; padding: 14px 16px; background: white; border-radius: 8px; border: 1px solid #cbd5e1;}
    .system-label{font-weight: 500; color: #64748b; font-size: 0.875rem;}
    .system-value{font-weight: 600; color: #1e40af; font-size: 1rem;}

    .charts-section{margin: 24px 0;}
    .charts-grid{display: grid; grid-template-columns: repeat(auto-fit, minmax(350px, 1fr)); gap: 20px;}
    .chart-container{padding: 20px; background: white; border-radius: 8px; border: 1px solid #cbd5e1;}
    .chart-container canvas{width: 100%; height: auto;}

    details summary{padding: 14px 18px; background: #1e40af; color: white; border-radius: 6px; margin-bottom: 16px; cursor: pointer; font-weight: 500; transition: all 0.2s;}
    details summary:hover{background: #1e3a8a;}
    details[open] summary{border-radius: 6px 6px 0 0;}
    
    /* Clean table styling */
    table{border-collapse: collapse; width:100%; border-radius: 8px; overflow: hidden; border: 1px solid #cbd5e1;}
    th{background: #f1f5f9; color: #475569; font-weight: 600; text-align: center; padding: 12px 10px; border-bottom: 2px solid #cbd5e1;}
    td{padding: 12px 10px; text-align: center; border-bottom: 1px solid #e2e8f0; color: #475569;}
    tr:hover{background-color: #f8fafc;}

    /* Professional sections */
    .section-header{display: flex; flex-direction: column; margin-bottom: 20px; padding-bottom: 12px; border-bottom: 2px solid #dbeafe;}
    .section-header h2{margin: 0 0 6px 0; color: #1e40af; font-size: 1.25rem; font-weight: 600;}
    .section-header .muted{margin: 0;}
    
    .form-group{display: flex; flex-direction: column; gap: 6px;}
    .form-group label{color: #475569; font-weight: 500; font-size: 0.875rem;}
    .form-input{padding: 10px 12px; border-radius: 6px; border: 1px solid #cbd5e1; background: white; color: #334155; font-size: 0.9375rem;}
    .form-input:focus{outline: none; border-color: #3b82f6; box-shadow: 0 0 0 3px rgba(59,130,246,0.1);}
    
    .btn-primary{background: #2563eb; color: white; border: none; padding: 10px 20px; border-radius: 6px; font-weight: 500; cursor: pointer; transition: all 0.2s;}
    .btn-primary:hover{background: #1d4ed8;}
    .btn-secondary{background: white; color: #475569; border: 1px solid #cbd5e1; padding: 10px 20px; border-radius: 6px; font-weight: 500; cursor: pointer; transition: all 0.2s;}
    .btn-secondary:hover{background: #f1f5f9; border-color: #94a3b8;}
    
    .log-container{margin-top: 16px; max-height: 220px; overflow-y: auto; background: #f8fafc; border-radius: 6px; padding: 12px; font-family: 'Courier New', monospace; font-size: 12px; color: #475569; border: 1px solid #cbd5e1;}

    .fota-settings{display: grid; grid-template-columns: 1fr 1fr; gap: 16px; margin-bottom: 20px;}
    .config-grid{display: grid; grid-template-columns: 1fr 1fr; gap: 16px; margin-bottom: 20px;}
    .write-fields{display: grid; grid-template-columns: 1fr 1fr auto; gap: 16px; align-items: end;}
    
    .register-selection{margin: 16px 0; padding: 16px; background: #f1f5f9; border-radius: 8px;}
    .register-selection h4{margin: 0 0 12px 0; color: #334155; font-size: 0.9rem;}
    .register-grid{display: grid; grid-template-columns: repeat(auto-fit, minmax(140px, 1fr)); gap: 8px;}
    .reg-checkbox{display: flex; align-items: center; gap: 8px; padding: 6px 8px; background: white; border-radius: 4px; cursor: pointer; font-size: 0.8rem;}
    .reg-checkbox input[type="checkbox"]{margin: 0; width: 14px; height: 14px;}
    .reg-checkbox:hover{background: #e2e8f0;}

    .actions-row{display: flex; align-items: center; gap: 12px; flex-wrap: wrap;}
    .fota-progress{display: none; height: 6px; border-radius: 3px; background: #e2e8f0; overflow: hidden;}
  </style>
</head>
<body>
  <h1>ESP32 Console — FOTA, Config, Write & Telemetry</h1>

  <!-- TOP: Data logs only -->
  <div class="card">
    <div class="section-header">
      <h2>Data Logs (Top)</h2>
      <div class="muted">Receive-only logs from <code>{{topic_data}}</code>. Charts & raw table remain below.</div>
    </div>
    <div id="dataTopLog" class="log-container"></div>
  </div>

  <div class="grid">
    <!-- FOTA -->
    <div class="card">
      <div class="section-header">
        <h2>Firmware Upload & Update</h2>
        <div class="muted">Downlink to <code>{{topic_cmd}}</code>; status from <code>{{topic_stat}}</code>; ACKs on <code>{{topic_ack_fota}}</code>.</div>
      </div>
      <form id="uploadForm">
        <div class="file-upload-area">
          <input type="file" id="fw" accept=".bin" required />
          <div class="file-info">Select .bin firmware file</div>
        </div>
        <div class="fota-settings">
          <div class="form-group">
            <label>Version</label>
            <input type="text" id="version" value="v1.0.0" class="form-input"/>
          </div>
          <div class="form-group">
            <label>Chunk Size</label>
            <input type="number" id="chunk" value="4096" min="512" step="512" class="form-input"/>
          </div>
        </div>
        <div class="actions-row">
          <button type="submit" class="btn-primary">Start FOTA</button>
          <progress id="prog" value="0" max="100" class="fota-progress"></progress>
          <button type="button" id="rebootBtn" class="btn-secondary">Send Reboot</button>
        </div>
      </form>
      <div id="fotaLog" class="log-container"></div>
    </div>

    <!-- Device Configuration -->
    <div class="card">
      <div class="section-header">
        <h2>Device Configuration</h2>
        <div class="muted">Sends sealed JSON to <code>{{topic_config}}</code>; ACKs on <code>{{topic_ack_config}}</code>.</div>
      </div>
      <form id="cfgForm">
        <div class="config-grid">
          <div class="form-group"><label>Poll Period (ms)</label><input type="number" id="poll_period_ms" min="100" step="100" class="form-input" placeholder="10000"/></div>
          <div class="form-group"><label>Upload Period (ms)</label><input type="number" id="upload_period_ms" min="100" step="100" class="form-input" placeholder="20000"/></div>
          <div class="form-group"><label>Buffer Capacity</label><input type="number" id="buffer_capacity" min="1" step="1" class="form-input" placeholder="256"/></div>
        </div>
        
        <div class="register-selection">
          <h4>Register Request ID</h4>
          <div class="register-grid">
            <label class="reg-checkbox"><input type="checkbox" id="reg_0" checked/><span>AC Voltage</span></label>
            <label class="reg-checkbox"><input type="checkbox" id="reg_1" checked/><span>AC Current</span></label>
            <label class="reg-checkbox"><input type="checkbox" id="reg_2" checked/><span>Grid Frequency</span></label>
            <label class="reg-checkbox"><input type="checkbox" id="reg_3" checked/><span>PV1 Voltage</span></label>
            <label class="reg-checkbox"><input type="checkbox" id="reg_4" checked/><span>PV2 Voltage</span></label>
            <label class="reg-checkbox"><input type="checkbox" id="reg_5" checked/><span>PV1 Current</span></label>
            <label class="reg-checkbox"><input type="checkbox" id="reg_6" checked/><span>PV2 Current</span></label>
            <label class="reg-checkbox"><input type="checkbox" id="reg_7" checked/><span>Temperature</span></label>
            <label class="reg-checkbox"><input type="checkbox" id="reg_8" checked/><span>Export %</span></label>
            <label class="reg-checkbox"><input type="checkbox" id="reg_9" checked/><span>Output Power</span></label>
          </div>
        </div>
        <div class="actions-row">
          <button type="submit" class="btn-primary">Send Config</button>
          <button type="button" id="loadCfgBtn" class="btn-secondary">Load Current</button>
        </div>
      </form>
      <div id="cfgLog" class="log-container"></div>
    </div>
  </div>

  <!-- Write Register -->
  <div class="card">
    <div class="section-header">
      <h2>Write Register</h2>
      <div class="muted">Downlink to <code>{{topic_write}}</code>; ACKs on <code>{{topic_ack_write}}</code>.</div>
    </div>
    <form id="writeForm">
      <div class="write-fields">
        <div class="form-group"><label>Register Address</label><input type="number" id="wr_address" min="0" step="1" required class="form-input" placeholder="0"/></div>
        <div class="form-group"><label>Register Value</label><input type="number" id="wr_value" step="1" required class="form-input" placeholder="0"/></div>
        <button type="submit" class="btn-primary">Send Write</button>
      </div>
    </form>
    <div id="writeLog" class="log-container"></div>
  </div>

  <!-- Solar Data Dashboard -->
  <div class="card">
    <div class="section-header">
      <h2>Live Solar Data Dashboard</h2>
      <div class="muted">Real-time telemetry from <code>{{topic_data}}</code></div>
    </div>
    
    <!-- Status Overview -->
    <div class="status-grid">
      <div class="status-card"><div class="status-value" id="currentVoltage">--</div><div class="status-label">AC Voltage (V)</div></div>
      <div class="status-card"><div class="status-value" id="currentCurrent">--</div><div class="status-label">AC Current (A)</div></div>
      <div class="status-card"><div class="status-value" id="currentPower">--</div><div class="status-label">AC Power (W)</div></div>
      <div class="status-card"><div class="status-value" id="currentFreq">--</div><div class="status-label">Frequency (Hz)</div></div>
    </div>

    <!-- PV Panels -->
    <div class="pv-section">
      <h3 style="color: #1e40af; font-size: 1.125rem; font-weight: 600; margin-bottom: 16px;">PV Panel Status</h3>
      <div class="pv-grid">
        <div class="pv-card"><div class="pv-title">PV1</div><div class="pv-values"><span id="pv1Voltage">--</span> • <span id="pv1Current">--</span></div></div>
        <div class="pv-card"><div class="pv-title">PV2</div><div class="pv-values"><span id="pv2Voltage">--</span> • <span id="pv2Current">--</span></div></div>
        <div class="pv-card"><div class="pv-title">Output Power</div><div class="pv-values"><span id="outputPower">--</span>W</div></div>
      </div>
    </div>

    <!-- System Status -->
    <div class="system-grid">
      <div class="system-item"><span class="system-label">Temperature:</span><span id="sysTemp" class="system-value">--</span></div>
      <div class="system-item"><span class="system-label">Export Ratio:</span><span id="exportRatio" class="system-value">--</span></div>
      <div class="system-item"><span class="system-label">Last Update:</span><span id="lastUpdate" class="system-value">--:--:--</span></div>
    </div>

    <!-- Live Charts -->
    <div class="charts-section">
      <h3 style="color: #1e40af; font-size: 1.125rem; font-weight: 600; margin-bottom: 16px;">Live Charts - All 10 Registers</h3>
      <div class="charts-grid">
        <div class="chart-container"><canvas id="voltageChart" width="350" height="180"></canvas></div>
        <div class="chart-container"><canvas id="currentChart" width="350" height="180"></canvas></div>
        <div class="chart-container"><canvas id="frequencyChart" width="350" height="180"></canvas></div>
        <div class="chart-container"><canvas id="powerChart" width="350" height="180"></canvas></div>
        <div class="chart-container"><canvas id="pv1VChart" width="350" height="180"></canvas></div>
        <div class="chart-container"><canvas id="pv1IChart" width="350" height="180"></canvas></div>
        <div class="chart-container"><canvas id="pv2VChart" width="350" height="180"></canvas></div>
        <div class="chart-container"><canvas id="pv2IChart" width="350" height="180"></canvas></div>
        <div class="chart-container"><canvas id="outputPowerChart" width="350" height="180"></canvas></div>
        <div class="chart-container"><canvas id="temperatureChart" width="350" height="180"></canvas></div>
      </div>
    </div>

    <!-- Raw Data Table -->
    <details style="margin-top: 24px;">
      <summary>Raw Data Records</summary>
      <table id="dataTable" style="margin-top:16px">
        <thead><tr><th>Timestamp</th><th>V(AC)</th><th>I(AC)</th><th>Freq</th><th>PV1-V</th><th>PV1-I</th><th>PV2-V</th><th>PV2-I</th><th>Temp</th><th>Export%</th><th>Power(W)</th></tr></thead>
        <tbody></tbody>
      </table>
      <div class="muted" style="margin-top: 12px;">Showing most recent 50 records.</div>
    </details>
  </div>

<script>
const dataTopLog = document.getElementById('dataTopLog');
const fotaLog = document.getElementById('fotaLog');
const cfgLog  = document.getElementById('cfgLog');
const writeLog= document.getElementById('writeLog');

function dlog(line){ const p=document.createElement('div'); p.textContent=line; dataTopLog.prepend(p); }
function flog(line){ const p=document.createElement('div'); p.textContent=line; fotaLog.prepend(p); }
function clog(line){ const p=document.createElement('div'); p.textContent=line; cfgLog.prepend(p); }
function wlog(line){ const p=document.createElement('div'); p.textContent=line; writeLog.prepend(p); }
function fmt(ts){ const d=new Date(ts); return d.toLocaleString(); }

// Poll individual log endpoints
async function pollDataLogs(){
  try{
    const r = await fetch('/api/logs/data'); const j = await r.json();
    dataTopLog.innerHTML='';
    for (let i=0;i<j.events.length;i++){
      const e = j.events[i];
      dlog(`[${fmt(e.ts)}] ${e.topic}: ${JSON.stringify(e)}`);
    }
  }catch{}
}

async function pollFotaLogs(){
  try{
    const r = await fetch('/api/logs/fota'); const j = await r.json();
    fotaLog.innerHTML='';
    for (let i=0;i<j.events.length;i++){
      const e = j.events[i];
      flog(`[${fmt(e.ts)}] ${e.topic}: ${JSON.stringify(e)}`);
    }
  }catch{}
}

async function pollConfigLogs(){
  try{
    const r = await fetch('/api/logs/config'); const j = await r.json();
    cfgLog.innerHTML='';
    for (let i=0;i<j.events.length;i++){
      const e = j.events[i];
      clog(`[${fmt(e.ts)}] ${e.topic}: ${JSON.stringify(e)}`);
    }
  }catch{}
}

async function pollWriteLogs(){
  try{
    const r = await fetch('/api/logs/write'); const j = await r.json();
    writeLog.innerHTML='';
    for (let i=0;i<j.events.length;i++){
      const e = j.events[i];
      wlog(`[${fmt(e.ts)}] ${e.topic}: ${JSON.stringify(e)}`);
    }
  }catch{}
}

// Chart data storage for all 10 registers
let chartData = { voltage: [], current: [], frequency: [], power: [], pv1V: [], pv1I: [], pv2V: [], pv2I: [], temperature: [], outputPower: [] };
let timeLabels = [];

async function pollData(){
  try{
    const r = await fetch('/api/data?limit=50'); const j = await r.json();
    const records = j.records || [];
    if (records.length > 0) {
      const latest = records[records.length - 1];
      const regs = latest.registers || {};
      const voltage = regs['Vac1_L1_Phase_voltage']?.value || 0;
      const current = regs['Iac1_L1_Phase_current']?.value || 0;
      const frequency = regs['Fac1_L1_Phase_frequency']?.value || 0;
      const pv1_voltage = regs['Vpv1_PV1_input_voltage']?.value || 0;
      const pv2_voltage = regs['Vpv2_PV2_input_voltage']?.value || 0;
      const pv1_current = regs['Ipv1_PV1_input_current']?.value || 0;
      const pv2_current = regs['Ipv2_PV2_input_current']?.value || 0;
      const temperature = regs['Inverter_internal_temperature']?.value || 0;
      const export_power_pct = regs['Set_export_power_percentage']?.value || 0;
      const output_power = regs['Pac_L_Inverter_current_output_power']?.value || 0;
      const power = voltage * current;

      document.getElementById('currentVoltage').textContent = voltage.toFixed(1);
      document.getElementById('currentCurrent').textContent = current.toFixed(2);
      document.getElementById('currentPower').textContent = Math.round(power);
      document.getElementById('currentFreq').textContent = frequency.toFixed(1);

      document.getElementById('pv1Voltage').textContent = pv1_voltage.toFixed(1) + 'V';
      document.getElementById('pv1Current').textContent = pv1_current.toFixed(2) + 'A';
      document.getElementById('pv2Voltage').textContent = pv2_voltage.toFixed(1) + 'V';
      document.getElementById('pv2Current').textContent = pv2_current.toFixed(2) + 'A';

      document.getElementById('sysTemp').textContent = temperature.toFixed(1) + '°C';
      document.getElementById('outputPower').textContent = output_power.toFixed(0) + 'W';
      document.getElementById('exportRatio').textContent = export_power_pct.toFixed(0) + '%';
      document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();

      const now = new Date().toLocaleTimeString();
      chartData.voltage.push(voltage);
      chartData.current.push(current);
      chartData.frequency.push(frequency);
      chartData.power.push(power);
      chartData.pv1V.push(pv1_voltage);
      chartData.pv1I.push(pv1_current);
      chartData.pv2V.push(pv2_voltage);
      chartData.pv2I.push(pv2_current);
      chartData.temperature.push(temperature);
      chartData.outputPower.push(output_power);
      timeLabels.push(now);
      
      if (timeLabels.length > 20) {
        Object.keys(chartData).forEach(key => chartData[key].shift());
        timeLabels.shift();
      }
      updateAllCharts();
    }

    const tb = document.querySelector('#dataTable tbody'); tb.innerHTML='';
    records.slice(-50).reverse().forEach(rec=>{
      const tr = document.createElement('tr');
      const regs = rec.registers || {};
      let timestamp = rec.server_timestamp || rec.timestamp;
      if (timestamp < 946684800000) { timestamp = Date.now(); }
      const cells = [
        new Date(timestamp).toLocaleString('en-US',{year:'numeric',month:'2-digit',day:'2-digit',hour:'2-digit',minute:'2-digit',second:'2-digit'}),
        (regs['Vac1_L1_Phase_voltage']?.value || 0).toFixed(1),
        (regs['Iac1_L1_Phase_current']?.value || 0).toFixed(2),
        (regs['Fac1_L1_Phase_frequency']?.value || 0).toFixed(1),
        (regs['Vpv1_PV1_input_voltage']?.value || 0).toFixed(1),
        (regs['Ipv1_PV1_input_current']?.value || 0).toFixed(2),
        (regs['Vpv2_PV2_input_voltage']?.value || 0).toFixed(1),
        (regs['Ipv2_PV2_input_current']?.value || 0).toFixed(2),
        (regs['Inverter_internal_temperature']?.value || 0).toFixed(1),
        (regs['Set_export_power_percentage']?.value || 0).toFixed(0),
        (regs['Pac_L_Inverter_current_output_power']?.value || 0).toFixed(0)
      ];
      cells.forEach(cellData => { const td = document.createElement('td'); td.textContent = cellData; tr.appendChild(td); });
      tb.appendChild(tr);
    });
  }catch(e){console.error('Poll data error:', e);}
}

function updateAllCharts() {
  drawChart('voltageChart', chartData.voltage, timeLabels, 'AC Voltage (V)');
  drawChart('currentChart', chartData.current, timeLabels, 'AC Current (A)');
  drawChart('frequencyChart', chartData.frequency, timeLabels, 'Grid Frequency (Hz)');
  drawChart('powerChart', chartData.power, timeLabels, 'AC Power (W)');
  drawChart('pv1VChart', chartData.pv1V, timeLabels, 'PV1 Voltage (V)');
  drawChart('pv1IChart', chartData.pv1I, timeLabels, 'PV1 Current (A)');
  drawChart('pv2VChart', chartData.pv2V, timeLabels, 'PV2 Voltage (V)');
  drawChart('pv2IChart', chartData.pv2I, timeLabels, 'PV2 Current (A)');
  drawChart('outputPowerChart', chartData.outputPower, timeLabels, 'Output Power (W)');
  drawChart('temperatureChart', chartData.temperature, timeLabels, 'Temperature (°C)');
}

function drawChart(canvasId, data, labels, title) {
  const canvas = document.getElementById(canvasId);
  if (!canvas) return;
  const ctx = canvas.getContext('2d');
  const width = canvas.width, height = canvas.height;
  ctx.clearRect(0, 0, width, height);
  ctx.fillStyle = '#ffffff'; ctx.fillRect(0, 0, width, height);
  ctx.strokeStyle = '#cbd5e1'; ctx.lineWidth = 1; ctx.strokeRect(0, 0, width, height);

  if (data.length < 1) {
    ctx.fillStyle = '#64748b'; ctx.font = '14px sans-serif'; ctx.textAlign = 'center';
    ctx.fillText('No Data Available', width / 2, height / 2);
    ctx.fillStyle = '#1e40af'; ctx.font = 'bold 14px sans-serif';
    ctx.fillText(title, width / 2, 25);
    return;
  }

  const minVal = Math.min(...data) * 0.98;
  const maxVal = Math.max(...data) * 1.02;
  const range = maxVal - minVal || 1;

  ctx.fillStyle = '#1e40af'; ctx.font = 'bold 14px sans-serif'; ctx.textAlign = 'center';
  ctx.fillText(title, width / 2, 18);

  ctx.strokeStyle = '#e2e8f0'; ctx.lineWidth = 0.5;
  for (let i = 1; i <= 4; i++) {
    const y = 35 + (height - 65) * i / 5;
    ctx.beginPath(); ctx.moveTo(45, y); ctx.lineTo(width - 15, y); ctx.stroke();
  }

  ctx.strokeStyle = '#64748b'; ctx.lineWidth = 1;
  ctx.beginPath(); ctx.moveTo(45, 30); ctx.lineTo(45, height - 25); ctx.stroke();
  ctx.beginPath(); ctx.moveTo(45, height - 25); ctx.lineTo(width - 15, height - 25); ctx.stroke();

  if (data.length < 2) return;

  ctx.fillStyle = '#3b82f6' + '15';
  ctx.beginPath(); ctx.moveTo(45, height - 25);
  data.forEach((value, index) => {
    const x = 45 + (width - 60) * index / (data.length - 1);
    const y = height - 25 - ((value - minVal) / range) * (height - 60);
    ctx.lineTo(x, y);
  });
  ctx.lineTo(45 + (width - 60), height - 25);
  ctx.closePath(); ctx.fill();

  ctx.strokeStyle = '#2563eb'; ctx.lineWidth = 2;
  ctx.beginPath();
  data.forEach((value, index) => {
    const x = 45 + (width - 60) * index / (data.length - 1);
    const y = height - 25 - ((value - minVal) / range) * (height - 60);
    if (index === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
  });
  ctx.stroke();

  ctx.fillStyle = '#2563eb';
  data.forEach((value, index) => {
    const x = 45 + (width - 60) * index / (data.length - 1);
    const y = height - 25 - ((value - minVal) / range) * (height - 60);
    ctx.beginPath(); ctx.arc(x, y, 2, 0, 2 * Math.PI); ctx.fill();
  });

  ctx.fillStyle = '#64748b'; ctx.font = '10px sans-serif'; ctx.textAlign = 'right';
  for (let i = 0; i <= 4; i++) {
    const value = minVal + (range * (4 - i) / 4);
    const y = 35 + (height - 65) * i / 5;
    ctx.fillText(value.toFixed(1), 40, y + 3);
  }

  if (data.length > 0) {
    const currentVal = data[data.length - 1];
    ctx.fillStyle = '#1e40af'; ctx.fillRect(width - 70, 3, 65, 20);
    ctx.fillStyle = 'white'; ctx.font = 'bold 11px sans-serif'; ctx.textAlign = 'center';
    ctx.fillText(currentVal.toFixed(1), width - 37.5, 16);
  }

  if (labels.length >= 2) {
    ctx.fillStyle = '#64748b'; ctx.font = '9px sans-serif';
    ctx.textAlign = 'left';
    const firstTime = labels[0].split(':').slice(1, 3).join(':');
    ctx.fillText(firstTime, 47, height - 10);
    ctx.textAlign = 'right';
    const lastTime = labels[labels.length - 1].split(':').slice(1, 3).join(':');
    ctx.fillText(lastTime, width - 17, height - 10);
  }
}

setInterval(pollDataLogs, 1500);
setInterval(pollFotaLogs, 1500);
setInterval(pollConfigLogs, 1500);
setInterval(pollWriteLogs, 1500);
setInterval(pollData, 1500);
pollDataLogs(); pollFotaLogs(); pollConfigLogs(); pollWriteLogs(); pollData();

// Reboot button
document.getElementById('rebootBtn').onclick = async ()=>{
  const r = await fetch('/api/reboot', {method:'POST'}); const j = await r.json();
  flog('✓ Reboot sent: '+JSON.stringify(j));
};

// FOTA form
document.getElementById('uploadForm').onsubmit = async (e)=>{
  e.preventDefault();
  const fw = document.getElementById('fw').files[0];
  const btn = e.target.querySelector('.btn-primary');
  const originalText = btn.textContent;
  const version = document.getElementById('version').value || 'v1.0.0';
  const chunk = parseInt(document.getElementById('chunk').value || '4096',10);
  if (!fw){ flog('⚠ Please select a .bin firmware file'); return; }
  btn.textContent = 'Uploading...'; btn.disabled = true;
  const fd = new FormData(); fd.append('firmware', fw); fd.append('version', version); fd.append('chunk', chunk);
  const prog = document.getElementById('prog'); prog.style.display='inline-block'; prog.value=0;
  flog(`▶ Starting FOTA upload: ${fw.name} (${(fw.size/1024).toFixed(1)}KB)`);
  try {
    const r = await fetch('/api/fota/upload', {method:'POST', body: fd});
    const j = await r.json(); 
    flog('✓ FOTA started: '+JSON.stringify(j)); 
    setTimeout(()=>prog.value=20, 300);
    btn.textContent = '✓ Started';
    setTimeout(() => { btn.textContent = originalText; btn.disabled = false; }, 3000);
  } catch (error) {
    flog('✗ FOTA upload failed: ' + error.message);
    btn.textContent = '✗ Failed';
    setTimeout(() => { btn.textContent = originalText; btn.disabled = false; }, 3000);
  }
};

// Helper function to create register request ID from checkboxes
function getRegisterRequestId() {
  let regId = 0;
  for (let i = 0; i < 10; i++) {
    if (document.getElementById(`reg_${i}`)?.checked) {
      regId |= (1 << i);  // Set bit i if checkbox is checked
    }
  }
  return regId;
}

// Helper function to set checkboxes from register request ID
function setRegisterCheckboxes(regId) {
  for (let i = 0; i < 10; i++) {
    const checkbox = document.getElementById(`reg_${i}`);
    if (checkbox) {
      checkbox.checked = (regId & (1 << i)) !== 0;
    }
  }
}

// Config UI
async function loadConfig(){
  const r = await fetch('/api/config'); const j = await r.json();
  for (const k of ['poll_period_ms','upload_period_ms','buffer_capacity']){
    if (j.config[k] !== undefined) document.getElementById(k).value = j.config[k];
  }
  if (j.config['reg_req_id_1'] !== undefined) {
    setRegisterCheckboxes(j.config['reg_req_id_1']);
  }
  clog('✓ Loaded config');
}
document.getElementById('loadCfgBtn').onclick = loadConfig;

document.getElementById('cfgForm').onsubmit = async (e)=>{
  e.preventDefault();
  const btn = e.target.querySelector('.btn-primary');
  const originalText = btn.textContent;
  btn.textContent = 'Sending...'; btn.disabled = true;
  const body = {};
  for (const k of ['poll_period_ms','upload_period_ms','buffer_capacity']){
    const v = document.getElementById(k).value;
    if (v !== '') body[k] = Number(v);
  }
  // Add register request ID from checkboxes
  body['reg_req_id_1'] = getRegisterRequestId();
  try {
    const r = await fetch('/api/config', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify(body)});
    const j = await r.json();
    clog('✓ Config sent: '+JSON.stringify(j.sent));
    btn.textContent = '✓ Sent';
    setTimeout(() => { btn.textContent = originalText; btn.disabled = false; }, 2000);
  } catch (error) {
    clog('✗ Config failed: ' + error.message);
    btn.textContent = '✗ Failed';
    setTimeout(() => { btn.textContent = originalText; btn.disabled = false; }, 2000);
  }
};

// Write UI
document.getElementById('writeForm').onsubmit = async (e)=>{
  e.preventDefault();
  const btn = e.target.querySelector('.btn-primary');
  const originalText = btn.textContent;
  const address = Number(document.getElementById('wr_address').value);
  const value   = Number(document.getElementById('wr_value').value);
  if (Number.isNaN(address) || Number.isNaN(value)) { wlog('⚠ Address and value must be valid numbers'); return; }
  btn.textContent = 'Writing...'; btn.disabled = true;
  try {
    const r = await fetch('/api/write', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({address, value})});
    const j = await r.json();
    wlog(`✓ Write sent: ${JSON.stringify(j.sent)}`);
    btn.textContent = '✓ Sent';
    setTimeout(() => { btn.textContent = originalText; btn.disabled = false; }, 2000);
  } catch (error) {
    wlog('✗ Write failed: ' + error.message);
    btn.textContent = '✗ Failed';
    setTimeout(() => { btn.textContent = originalText; btn.disabled = false; }, 2000);
  }
};
</script>
</body>
</html>"""

@app.route("/")
def index():
    return (INDEX_HTML
            .replace("{{topic_cmd}}", TOPIC_CMD)
            .replace("{{topic_stat}}", TOPIC_STAT)
            .replace("{{topic_config}}", TOPIC_CONFIG)
            .replace("{{topic_data}}", TOPIC_DATA)
            .replace("{{topic_write}}", TOPIC_WRITE)
            .replace("{{topic_ack_fota}}", TOPIC_ACK_FOTA)
            .replace("{{topic_ack_config}}", TOPIC_ACK_CONFIG)
            .replace("{{topic_ack_write}}", TOPIC_ACK_WRITE)
            )

# ---- Logs/Data APIs ----
@app.route("/api/logs/data")
def api_logs_data():
    return jsonify({"events": logs_data})

@app.route("/api/logs/fota")
def api_logs_fota():
    return jsonify({"events": logs_fota})

@app.route("/api/logs/config")
def api_logs_config():
    return jsonify({"events": logs_conf})

@app.route("/api/logs/write")
def api_logs_write():
    return jsonify({"events": logs_write})

@app.route("/api/data")
def api_data():
    limit = int(request.args.get("limit", "200"))
    recent_records = data_records[-limit:]
    return jsonify({"records": recent_records})

@app.route("/api/status")
def api_status():
    return jsonify({
        "device": DEV_ID,
        "broker": f"{BROKER_HOST}:{BROKER_PORT}",
        "topics": {
            "cmd": TOPIC_CMD, "status": TOPIC_STAT, "data": TOPIC_DATA,
            "config": TOPIC_CONFIG, "write": TOPIC_WRITE,
            "ack_fota": TOPIC_ACK_FOTA, "ack_config": TOPIC_ACK_CONFIG, "ack_write": TOPIC_ACK_WRITE
        },
        "fota_active": state["active"],
        "next_offset": state["next_offset"],
        "total": state["total"]
    })

# ---- Reboot ----
@app.route("/api/reboot", methods=["POST"])
def api_reboot():
    publish_cmd({"op": "reboot"})
    _push_log(logs_fota, {"topic":"fota/cmd", "dir":"tx", "info":"reboot_sent"})
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
    publish_cmd(manifest)
    _push_log(logs_fota, {"topic":"fota/cmd", "dir":"tx", "info":"manifest_sent", "size": len(fw), "sha256_hex": sha_hex, "chunk": chunk})
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
    _push_log(logs_conf, {"topic":"config", "dir":"tx", "sent": current_config.copy()})
    return jsonify({"ok": True, "sent": current_config})

# ---- WRITE: address/value -> publish sealed JSON ----
def make_write_payload(address: int, value: int) -> Dict[str, Any]:
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
    _push_log(logs_write, {"topic":"write", "dir":"tx", "sent": payload})
    return jsonify({"ok": True, "sent": payload})

# ---------- favicon ----------
@app.route("/favicon.ico")
def favicon():
    return ("", 204)

if __name__ == "__main__":
    time.sleep(0.5)
    app.run(host="0.0.0.0", port=8080, debug=False)
