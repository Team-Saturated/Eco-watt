#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os, io, time, json, hmac, base64, hashlib, struct, threading, queue, tempfile
from typing import Dict, Any, List, Tuple
from flask import Flask, request, jsonify, send_from_directory, Response
import paho.mqtt.client as mqtt
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

# =================== CONFIG ===================
BROKER_HOST = "broker.emqx.io"      # or "localhost"
BROKER_PORT = 1883
MQTT_USER   = None                   # set if needed
MQTT_PASS   = None

DEV_ID      = "esp32-01"             # MUST exactly match the deviceId used in sec.begin(...)
TOPIC_CMD   = f"devices/{DEV_ID}/fota/cmd"    # downlink (server -> device)
TOPIC_STAT  = f"devices/{DEV_ID}/fota/status" # uplink status (device -> server)
TOPIC_DATA  = f"devices/{DEV_ID}/data/dulmin"        # uplink data (device -> server)

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

def seal_downlink(obj: Dict[str, Any], msg_type: int = 2) -> str:
    """
    Creates a sealed, base64-encoded packet for downlink commands.
    Mirrors your Node-RED encryptor exactly.
    """
    global downlink
    Kenc, Kmac = derive_keys(DEV_ID, PSK_HEX)
    st = downlink.get(DEV_ID, {"boot": 0, "seq": 0})
    if st["boot"] == 0:
        # use ms granularity like JS Date.now(); ensure "new" boot vs prior senders
        st["boot"] = int(time.time() * 1000) + 10_000_000
        st["seq"]  = 0
    st["seq"] += 1
    downlink[DEV_ID] = st

    iv12 = os.urandom(12)
    hdr  = struct.pack("<BBHQQ", 1, msg_type & 0xFF, 0, st["boot"], st["seq"]) + iv12
    plain = json.dumps(obj, separators=(",", ":")).encode("utf-8")
    ct  = aes_ctr_crypt(Kenc, iv12, plain)
    mac = hmac.new(Kmac, hdr + ct, hashlib.sha256).digest()
    return base64.b64encode(hdr + ct + mac).decode("ascii")

def try_open_uplink(sealed_b64: bytes) -> Dict[str, Any] | None:
    """
    Decrypt & verify an uplink message from ESP32 (status/data).
    Returns a dict if JSON decodes; else None.
    """
    try:
        raw = base64.b64decode(sealed_b64, validate=True)
    except Exception:
        # maybe plaintext (legacy)
        try:
            return json.loads(sealed_b64.decode("utf-8"))
        except Exception:
            return None

    if len(raw) < HDR_LEN + MAC_LEN:
        return None

    hdr = raw[:HDR_LEN]; ct = raw[HDR_LEN:-MAC_LEN]; mac = raw[-MAC_LEN:]
    # derive
    Kenc, Kmac = derive_keys(DEV_ID, PSK_HEX)
    tag = hmac.new(Kmac, hdr + ct, hashlib.sha256).digest()
    if not hmac.compare_digest(tag, mac):
        return None

    iv12 = hdr[-12:]
    plain = aes_ctr_crypt(Kenc, iv12, ct)  # CTR decrypt = encrypt
    try:
        return json.loads(plain.decode("utf-8"))
    except Exception:
        return None

# ---------- Delta decompressor (Node-RED -> Python) ----------
def get16_le(buf: bytes, i: int):
    if i + 2 > len(buf): return None
    return buf[i] | (buf[i+1] << 8)

def get32_le(buf: bytes, i: int):
    if i + 4 > len(buf): return None
    return (buf[i] | (buf[i+1]<<8) | (buf[i+2]<<16) | (buf[i+3]<<24)) & 0xFFFFFFFF

def get64_le(buf: bytes, i: int):
    if i + 8 > len(buf): return None
    low = get32_le(buf, i)
    high = get32_le(buf, i+4)
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

def decompress_delta(hex_string: str) -> List[Dict[str, Any]] | None:
    buf = bytes.fromhex(hex_string)
    i = 0
    base_ts = get64_le(buf, i);  i += 8
    if base_ts is None: return None
    nrecs   = get16_le(buf, i);  i += 2
    if nrecs is None: return None

    out = []
    prev_ts = base_ts
    for _ in range(nrecs):
        dt_ms = get32_le(buf, i); i += 4
        qty   = get16_le(buf, i); i += 2
        if dt_ms is None or qty is None: return None
        need = qty * 4
        if i + need > len(buf): return None
        ts_ms = prev_ts + dt_ms

        regs: Dict[str, Any] = {}
        for __ in range(qty):
            addr = get16_le(buf, i)
            val  = get16_le(buf, i+2)
            if addr is None or val is None: return None
            d = decode_register(addr, val)
            regs[d["name"]] = {"address": d["address"], "raw_value": d["raw_value"], "value": d["value"], "unit": d["unit"]}
            i += 4

        out.append({"timestamp": ts_ms, "register_count": qty, "registers": regs})
        prev_ts = ts_ms
    return out

# ---------- State (events + data) ----------
fota_events: List[Dict[str, Any]] = []
data_records: List[Dict[str, Any]] = []
downlink: Dict[str, Dict[str, int]] = {}  # for boot/seq

def push_event(ev: Dict[str, Any]):
    fota_events.append({"ts": int(time.time()*1000), **ev})
    if len(fota_events) > MAX_EVENTS:
        del fota_events[:len(fota_events)-MAX_EVENTS]

def push_data(records: List[Dict[str, Any]]):
    data_records.extend(records)
    if len(data_records) > MAX_RECORDS:
        del data_records[:len(data_records)-MAX_RECORDS]

# ---------- MQTT wiring ----------
mqttc = mqtt.Client()
if MQTT_USER and MQTT_PASS:
    mqttc.username_pw_set(MQTT_USER, MQTT_PASS)

def on_connect(client, userdata, flags, rc):
    client.subscribe([(TOPIC_STAT, 1), (TOPIC_DATA, 1)])
    print(f"[MQTT] connected rc={rc}; subscribed to {TOPIC_STAT} and {TOPIC_DATA}")

def on_message(client, userdata, msg):
    if msg.topic == TOPIC_STAT:
        evt = try_open_uplink(msg.payload)
        if not evt:
            print("[FOTA][srv] could not parse status")
            return
        # record + react to events we care about
        push_event({"topic": "fota/status", **evt})
        ev = evt.get("ev")
        if ev == "need_chunks":
            state["next_offset"] = int(evt.get("next_offset", 0))
            state["total"]       = int(evt.get("total", 0))
            _send_next_chunk()
        elif ev == "progress":
            state["next_offset"] = int(evt.get("next_offset", 0))
            _send_next_chunk()
        elif ev in ("verify_ok", "verify_fail", "error", "rebooting"):
            pass

    elif msg.topic == TOPIC_DATA:
        obj = try_open_uplink(msg.payload)
        if not obj:
            print("[DATA][srv] could not parse data packet")
            return
        # Expect your ESP32 to send something like: {"type":"delta_v1","payload_hex":"...","compressed_bytes":...}
        if "payload_hex" in obj:
            recs = decompress_delta(obj["payload_hex"])
            if recs is not None:
                push_data(recs)
                push_event({"topic": "data", "info": f"decoded {len(recs)} records"})
        else:
            # if device sometimes sends plain JSON measurement
            push_event({"topic": "data", "info": "rx json", "preview": obj})

mqttc.on_connect = on_connect
mqttc.on_message = on_message
mqtt_thr = threading.Thread(target=lambda: mqttc.connect(BROKER_HOST, BROKER_PORT) or mqttc.loop_forever(), daemon=True)
mqtt_thr.start()

# ---------- FOTA state & helpers ----------
state = {
    "firmware": None, "version": None, "chunk": DEFAULT_CHUNK,
    "sha_hex": None, "nonce": None, "next_offset": 0, "total": 0, "active": False
}

def publish_cmd(obj: Dict[str, Any], msg_type: int = 2):
    sealed = seal_downlink(obj, msg_type=msg_type)
    mqttc.publish(TOPIC_CMD, sealed, qos=1, retain=False)

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
  <title>ESP32 FOTA & Telemetry</title>
  <meta name="viewport" content="width=device-width,initial-scale=1"/>
  <style>
    body{font-family: ui-sans-serif,system-ui,-apple-system,Segoe UI; margin: 2rem; color:#111;}
    h1{margin:0 0 1rem 0}
    .card{border:1px solid #ddd; border-radius:12px; padding:16px; margin-bottom:16px; box-shadow:0 1px 2px rgba(0,0,0,0.04)}
    button{padding:8px 14px; border-radius:10px; border:1px solid #ccc; background:#fafafa; cursor:pointer}
    button:hover{background:#f0f0f0}
    table{border-collapse: collapse; width:100%;}
    th,td{border-bottom:1px solid #eee; padding:8px; text-align:left}
    .muted{color:#666; font-size:12px}
    input[type=file]{padding:8px}
    progress{width: 220px;}
    code{background:#f6f8fa; padding:2px 6px; border-radius:6px}
    .grid{display:grid; grid-template-columns: 1fr 1fr; gap:16px}
    @media(max-width: 900px){ .grid{grid-template-columns: 1fr} }
  </style>
</head>
<body>
  <h1>ESP32 FOTA & Telemetry</h1>

  <div class="grid">
    <div class="card">
      <h2>Firmware Upload & Update</h2>
      <form id="uploadForm">
        <input type="file" id="fw" accept=".bin" required />
        <div style="margin-top:8px">
          <label>Version:&nbsp;<input type="text" id="version" value="v1.0.0"/></label>
          <label style="margin-left:12px">Chunk:&nbsp;<input type="number" id="chunk" value="4096" min="512" step="512"/></label>
        </div>
        <div style="margin-top:12px">
          <button type="submit">Start FOTA</button>
          <progress id="prog" value="0" max="100" style="vertical-align: middle; display:none"></progress>
        </div>
      </form>
      <div class="muted" style="margin-top:8px">Publishes <code>manifest → chunk(s) → finish</code> to <code>{{topic_cmd}}</code></div>
      <div id="fotaLog" style="margin-top:12px; max-height:220px; overflow:auto; font-family:ui-monospace,monospace; font-size:12px; background:#f8f9fb; padding:8px; border-radius:8px;"></div>
      <div style="margin-top:8px">
        <button id="rebootBtn">Send Reboot</button>
      </div>
    </div>

    <div class="card">
      <h2>Live Data</h2>
      <div class="muted">Decoding sealed <code>{{topic_data}}</code> uplink (SecureLink) using your delta format.</div>
      <table id="dataTable" style="margin-top:12px">
        <thead>
          <tr><th>Timestamp (ms)</th><th>Registers (decoded)</th></tr>
        </thead>
        <tbody></tbody>
      </table>
      <div class="muted">Showing most recent 50 records.</div>
    </div>
  </div>

<script>
const logEl = document.getElementById('fotaLog');
function log(line){ const p=document.createElement('div'); p.textContent=line; logEl.prepend(p); }
function fmt(ts){ const d=new Date(ts); return d.toLocaleString(); }

async function pollEvents(){
  try{
    const r = await fetch('/api/fota/events');
    const j = await r.json();
    logEl.innerHTML='';
    for (let i=j.events.length-1;i>=0;i--){
      const e = j.events[i];
      log(`[${fmt(e.ts)}] ${e.topic}: ${JSON.stringify(e)}`);
    }
  }catch(e){ /* ignore */ }
}

async function pollData(){
  try{
    const r = await fetch('/api/data?limit=50');
    const j = await r.json();
    const tb = document.querySelector('#dataTable tbody');
    tb.innerHTML='';
    (j.records || []).slice(-50).reverse().forEach(rec=>{
      const tr = document.createElement('tr');
      const td1 = document.createElement('td'); td1.textContent = rec.timestamp;
      const td2 = document.createElement('td');
      const regs = rec.registers || {};
      const parts = [];
      for (const [k,v] of Object.entries(regs)){
        parts.push(`${k}: ${v.value} ${v.unit}`);
      }
      td2.textContent = parts.join('  |  ');
      tr.appendChild(td1); tr.appendChild(td2);
      tb.appendChild(tr);
    });
  }catch(e){ /* ignore */ }
}

setInterval(pollEvents, 1500);
setInterval(pollData, 1500);
pollEvents(); pollData();

document.getElementById('rebootBtn').onclick = async ()=>{
  const r = await fetch('/api/reboot', {method:'POST'});
  const j = await r.json();
  log('reboot sent: '+JSON.stringify(j));
};

document.getElementById('uploadForm').onsubmit = async (e)=>{
  e.preventDefault();
  const fw = document.getElementById('fw').files[0];
  const version = document.getElementById('version').value || 'v1.0.0';
  const chunk = parseInt(document.getElementById('chunk').value || '4096',10);
  if (!fw){ alert('Pick a .bin'); return; }

  const fd = new FormData();
  fd.append('firmware', fw);
  fd.append('version', version);
  fd.append('chunk', chunk);

  const prog = document.getElementById('prog'); prog.style.display='inline-block'; prog.value=0;
  log('Uploading firmware and starting FOTA...');

  const r = await fetch('/api/fota/upload', {method:'POST', body: fd});
  const j = await r.json();
  log('start: '+JSON.stringify(j));
  // progress is on MQTT events; bump fake progress bar on manifest ack
  setTimeout(()=>prog.value=20, 300);
};
</script>
</body>
</html>
"""

@app.route("/")
def index():
    return INDEX_HTML.replace("{{topic_cmd}}", TOPIC_CMD).replace("{{topic_data}}", TOPIC_DATA)

# ---- API: events/data/status ----
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
        "topics": {"cmd": TOPIC_CMD, "status": TOPIC_STAT, "data": TOPIC_DATA},
        "fota_active": state["active"],
        "next_offset": state["next_offset"],
        "total": state["total"]
    })

# ---- API: reboot ----
@app.route("/api/reboot", methods=["POST"])
def api_reboot():
    publish_cmd({"op": "reboot"})
    push_event({"topic": "fota/status", "ev": "reboot_sent"})
    return jsonify({"ok": True})

# ---- API: upload + start FOTA ----
@app.route("/api/fota/upload", methods=["POST"])
def api_fota_upload():
    if "firmware" not in request.files:
        return jsonify({"ok": False, "error": "no_firmware"}), 400

    f = request.files["firmware"]
    version = request.form.get("version", "v1.0.0")
    try:
        chunk = int(request.form.get("chunk", str(DEFAULT_CHUNK)))
    except:
        chunk = DEFAULT_CHUNK

    # Save to temp and read
    fd, path = tempfile.mkstemp(prefix="fw_", suffix=".bin")
    os.close(fd)
    f.save(path)
    with open(path, "rb") as fh:
        fw = fh.read()
    os.remove(path)

    sha_hex = hashlib.sha256(fw).hexdigest().upper()
    nonce = os.urandom(16)

    state.update({"firmware": fw, "version": version, "chunk": chunk, "sha_hex": sha_hex,
                  "nonce": nonce, "next_offset": 0, "total": len(fw), "active": True})

    manifest = {"op": "manifest", "version": version, "size": len(fw), "chunk": chunk,
                "sha256_hex": sha_hex, "nonce_b64": base64.b64encode(nonce).decode("ascii")}
    publish_cmd(manifest)
    push_event({"topic": "fota/status", "ev": "manifest_sent", "size": len(fw), "sha256_hex": sha_hex, "chunk": chunk})
    return jsonify({"ok": True, "version": version, "size": len(fw), "sha256_hex": sha_hex, "chunk": chunk})

# ---------- static (optional) ----------
@app.route("/favicon.ico")
def favicon():
    return ("", 204)

if __name__ == "__main__":
    time.sleep(0.5)
    app.run(host="0.0.0.0", port=8080, debug=False)
