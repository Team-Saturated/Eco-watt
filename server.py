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
TOPIC_DATA  = f"devices/{DEV_ID}/data"           # uplink telemetry (device -> server)
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
    body{font-family: ui-sans-serif,system-ui,-apple-system,Segoe UI; margin: 2rem; color:#111;}
    h1{margin:0 0 1rem 0}
    .grid{display:grid; grid-template-columns: 1fr 1fr; gap:16px}
    @media(max-width: 1200px){ .grid{grid-template-columns: 1fr} }
    .card{border:1px solid #ddd; border-radius:12px; padding:16px; margin-bottom:16px; box-shadow:0 1px 2px rgba(0,0,0,0.04)}
    button{padding:8px 14px; border-radius:10px; border:1px solid #ccc; background:#fafafa; cursor:pointer}
    button:hover{background:#f0f0f0}
    table{border-collapse: collapse; width:100%;}
    th,td{border-bottom:1px solid #eee; padding:8px; text-align:left; vertical-align:top}
    .muted{color:#666; font-size:12px}
    input[type=file], input[type=text], input[type=number]{padding:8px}
    progress{width: 220px;}
    code{background:#f6f8fa; padding:2px 6px; border-radius:6px}
    .kv{display:flex; gap:10px; flex-wrap:wrap; align-items:center}
    .kv label{display:flex; align-items:center; gap:6px}
    .stack{display:flex; flex-direction:column; gap:8px}
  </style>
</head>
<body>
  <h1>ESP32 Console — FOTA, Config, Write & Telemetry</h1>

  <div class="grid">
    <div class="card">
      <h2>Firmware Upload & Update</h2>
      <form id="uploadForm">
        <input type="file" id="fw" accept=".bin" required />
        <div class="kv" style="margin-top:8px">
          <label>Version <input type="text" id="version" value="v1.0.0"/></label>
          <label>Chunk <input type="number" id="chunk" value="4096" min="512" step="512"/></label>
        </div>
        <div style="margin-top:12px">
          <button type="submit">Start FOTA</button>
          <progress id="prog" value="0" max="100" style="vertical-align: middle; display:none"></progress>
          <button type="button" id="rebootBtn" style="margin-left:8px">Send Reboot</button>
        </div>
      </form>
      <div class="muted" style="margin-top:8px">FOTA publishes sealed <code>manifest → chunk(s) → finish</code> to <code>{{topic_cmd}}</code></div>
      <div id="fotaLog" style="margin-top:12px; max-height:240px; overflow:auto; font-family:ui-monospace,monospace; font-size:12px; background:#f8f9fb; padding:8px; border-radius:8px;"></div>
    </div>

    <div class="card">
      <h2>Device Config</h2>
      <div class="muted">Sends a sealed JSON to <code>{{topic_config}}</code>. Only changed fields are updated in the form; the server merges and sends full config.</div>
      <form id="cfgForm" class="stack" style="margin-top:8px">
        <div class="kv">
          <label>poll_period_ms <input type="number" id="poll_period_ms" min="100" step="100"></label>
          <label>upload_period_ms <input type="number" id="upload_period_ms" min="100" step="100"></label>
        </div>
        <div class="kv">
          <label>buffer_capacity <input type="number" id="buffer_capacity" min="1" step="1"></label>
          <label>reg_req_id_1 <input type="number" id="reg_req_id_1" min="0" step="1"></label>
        </div>
        <div>
          <button type="submit">Send Config</button>
          <button type="button" id="loadCfgBtn" style="margin-left:8px">Load Current</button>
        </div>
      </form>
      <div id="cfgLog" style="margin-top:12px; max-height:160px; overflow:auto; font-family:ui-monospace,monospace; font-size:12px; background:#f8f9fb; padding:8px; border-radius:8px;"></div>
    </div>
  </div>

  <div class="card">
    <h2>Write Register</h2>
    <div class="muted">Publishes a sealed JSON to <code>{{topic_write}}</code> with address & value.</div>
    <form id="writeForm" class="kv" style="margin-top:8px">
      <label>address <input type="number" id="wr_address" min="0" step="1" required></label>
      <label>value <input type="number" id="wr_value" step="1" required></label>
      <button type="submit">Send Write</button>
    </form>
    <div id="writeLog" style="margin-top:12px; max-height:120px; overflow:auto; font-family:ui-monospace,monospace; font-size:12px; background:#f8f9fb; padding:8px; border-radius:8px;"></div>
  </div>

  <div class="card">
    <h2>Live Data</h2>
    <div class="muted">Decoding sealed <code>{{topic_data}}</code> uplink records.</div>
    <table id="dataTable" style="margin-top:12px">
      <thead><tr><th>Timestamp (ms)</th><th>Registers (decoded)</th></tr></thead>
      <tbody></tbody>
    </table>
    <div class="muted">Showing most recent 50 records.</div>
  </div>

<script>
const logEl = document.getElementById('fotaLog');
const cfgLog = document.getElementById('cfgLog');
const writeLog = document.getElementById('writeLog');
function log(line){ const p=document.createElement('div'); p.textContent=line; logEl.prepend(p); }
function clog(line){ const p=document.createElement('div'); p.textContent=line; cfgLog.prepend(p); }
function wlog(line){ const p=document.createElement('div'); p.textContent=line; writeLog.prepend(p); }
function fmt(ts){ const d=new Date(ts); return d.toLocaleString(); }

async function pollEvents(){
  try{
    const r = await fetch('/api/fota/events'); const j = await r.json();
    logEl.innerHTML='';
    for (let i=j.events.length-1;i>=0;i--){
      const e = j.events[i];
      log(`[${fmt(e.ts)}] ${e.topic}: ${JSON.stringify(e)}`);
    }
  }catch{}
}
async function pollData(){
  try{
    const r = await fetch('/api/data?limit=50'); const j = await r.json();
    const tb = document.querySelector('#dataTable tbody'); tb.innerHTML='';
    (j.records || []).slice(-50).reverse().forEach(rec=>{
      const tr = document.createElement('tr');
      const td1 = document.createElement('td'); td1.textContent = rec.timestamp;
      const td2 = document.createElement('td');
      const regs = rec.registers || {};
      td2.textContent = Object.entries(regs).map(([k,v])=>`${k}: ${v.value} ${v.unit}`).join('  |  ');
      tr.appendChild(td1); tr.appendChild(td2); tb.appendChild(tr);
    });
  }catch{}
}
setInterval(pollEvents, 1500);
setInterval(pollData, 1500);
pollEvents(); pollData();

document.getElementById('rebootBtn').onclick = async ()=>{
  const r = await fetch('/api/reboot', {method:'POST'}); const j = await r.json();
  log('reboot sent: '+JSON.stringify(j));
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
  const j = await r.json(); log('start: '+JSON.stringify(j)); setTimeout(()=>prog.value=20, 300);
};

// ---- Config UI ----
async function loadConfig(){
  const r = await fetch('/api/config'); const j = await r.json();
  for (const k of ['poll_period_ms','upload_period_ms','buffer_capacity','reg_req_id_1']){
    if (j.config[k] !== undefined) document.getElementById(k).value = j.config[k];
  }
  clog('loaded: '+JSON.stringify(j.config));
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
  clog('send: '+JSON.stringify(j));
};

// ---- Write Register UI ----
document.getElementById('writeForm').onsubmit = async (e)=>{
  e.preventDefault();
  const address = Number(document.getElementById('wr_address').value);
  const value   = Number(document.getElementById('wr_value').value);
  if (Number.isNaN(address) || Number.isNaN(value)) { alert('address/value must be numbers'); return; }
  const r = await fetch('/api/write', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({address, value})});
  const j = await r.json();
  wlog('write: '+JSON.stringify(j));
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
