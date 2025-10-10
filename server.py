#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import base64, hashlib, hmac, os, struct, threading, time, json
from flask import Flask, request, jsonify
import paho.mqtt.client as mqtt
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

# =================== CONFIG ===================
BROKER_HOST = "broker.emqx.io"      # or "localhost"
BROKER_PORT = 1883
MQTT_USER   = None                   # set if needed
MQTT_PASS   = None

DEV_ID      = "esp32-01"             # MUST match the deviceId passed to sec.begin(...)
TOPIC_CMD   = f"devices/{DEV_ID}/fota/cmd"
TOPIC_STAT  = f"devices/{DEV_ID}/fota/status"

# 32-byte PSK (hex) — MUST match the device's NVS PSK
PSK_HEX     = "4968A7E8835BC6EC5BDBE15AA9E7C478E5616E33AA0CC4CADB53A81AA20FA727"

DEFAULT_CHUNK = 4096
# ==============================================

app = Flask(__name__)

# ---------- SecureLink helpers (mirror your Node-RED logic) ----------
HDR_LEN = 1 + 1 + 2 + 8 + 8 + 12  # ver|type|reserved|boot|seq|iv12

def hkdf48(ikm: bytes, salt: bytes, info: bytes) -> bytes:
    prk = hmac.new(salt, ikm, hashlib.sha256).digest()
    T = b""; okm = b""; ctr = 0
    while len(okm) < 48:
        ctr += 1
        T = hmac.new(prk, T + info + bytes([ctr]), hashlib.sha256).digest()
        okm += T
    return okm[:48]

def derive_keys(device_id: str, psk_hex: str):
    salt = hashlib.sha256(device_id.encode("utf-8")).digest()
    okm  = hkdf48(bytes.fromhex(psk_hex), salt, b"EcoWatt")
    Kenc = okm[:16]
    Kmac = okm[16:48]
    return Kenc, Kmac

def aes_ctr_encrypt(Kenc: bytes, iv12: bytes, plaintext: bytes) -> bytes:
    ctr16 = iv12 + b"\x00\x00\x00\x00"  # iv12 || 4 zero bytes
    enc = Cipher(algorithms.AES(Kenc), modes.CTR(ctr16)).encryptor()
    return enc.update(plaintext) + enc.finalize()

# ---------- MQTT client ----------
mqttc = mqtt.Client()
if MQTT_USER and MQTT_PASS:
    mqttc.username_pw_set(MQTT_USER, MQTT_PASS)

# In-flight FOTA state
state = {
    "firmware": None,        # bytes
    "version":  None,        # str
    "chunk":    DEFAULT_CHUNK,
    "sha_hex":  None,
    "nonce":    None,        # bytes(16)
    "next_offset": 0,
    "total":       0,
    "active":      False
}

# Per-device downlink anti-replay counters (boot, seq) to mirror your JS
downlink = {}  # deviceId -> {"boot": int, "seq": int}

def publish_cmd(obj: dict, msg_type: int = 2, device_id: str = DEV_ID):
    # Derive keys just like your Node-RED function (per deviceId)
    Kenc, Kmac = derive_keys(device_id, PSK_HEX)

    # Per-device counters
    st = downlink.get(device_id, {"boot": 0, "seq": 0})
    if st["boot"] == 0:
        # Using ms granularity like BigInt(Date.now()) in your JS
        st["boot"] = int(time.time() * 1000)
        st["seq"]  = 0
    st["seq"] += 1
    downlink[device_id] = st

    # Header: <BBHQQ> then append iv12 (LE packing)
    iv12 = os.urandom(12)
    hdr  = struct.pack("<BBHQQ", 1, msg_type & 0xFF, 0, st["boot"], st["seq"]) + iv12

    # Compact JSON (matches your JSON.stringify)
    plain = json.dumps(obj, separators=(",", ":")).encode("utf-8")

    # Encrypt + HMAC(header||ct)
    ct  = aes_ctr_encrypt(Kenc, iv12, plain)
    mac = hmac.new(Kmac, hdr + ct, hashlib.sha256).digest()

    sealed_b64 = base64.b64encode(hdr + ct + mac).decode("ascii")
    mqttc.publish(TOPIC_CMD, sealed_b64, qos=1, retain=False)

def on_connect(client, userdata, flags, rc):
    client.subscribe(TOPIC_STAT, qos=1)
    print(f"[MQTT] connected rc={rc}, subscribed {TOPIC_STAT}")

def on_message(client, userdata, msg):
    payload = msg.payload
    # Try sealed (b64) → open with our keys; fallback to plaintext JSON
    evt = None
    try:
        sealed = base64.b64decode(payload, validate=True)
        # We don't know msg_type here, but we don't need it; just decrypt with derived keys.
        # Device status is typically sealed with the same KDF; if plaintext, fallback below.
        # We'll try both: current deviceId and DEV_ID (usually same).
        for did in (DEV_ID,):
            Kenc, Kmac = derive_keys(did, PSK_HEX)
            if len(sealed) >= HDR_LEN + 32:
                hdr = sealed[:HDR_LEN]
                body = sealed[HDR_LEN:-32]
                tag  = sealed[-32:]
                if hmac.compare_digest(hmac.new(Kmac, hdr + body, hashlib.sha256).digest(), tag):
                    iv12 = hdr[-12:]
                    plain = aes_ctr_encrypt(Kenc, iv12, body)  # CTR decrypt = encrypt
                    evt = json.loads(plain.decode("utf-8"))
                    break
    except Exception:
        pass

    if evt is None:
        try:
            evt = json.loads(payload.decode("utf-8"))
        except Exception:
            print("[FOTA][srv] status parse error")
            return

    handle_status(evt)

def handle_status(evt: dict):
    ev = evt.get("ev")
    if ev == "need_chunks":
        state["next_offset"] = int(evt.get("next_offset", 0))
        state["total"]       = int(evt.get("total", 0))
        print(f"[FOTA][srv] device ready, next={state['next_offset']}/{state['total']}")
        _send_next_chunk()
    elif ev == "progress":
        state["next_offset"] = int(evt.get("next_offset", 0))
        pct = evt.get("pct")
        print(f"[FOTA][srv] progress: {state['next_offset']}/{state['total']} ({pct}%)")
        _send_next_chunk()
    elif ev == "verify_ok":
        print("[FOTA][srv] verify OK:", evt.get("sha256_hex"))
    elif ev == "verify_fail":
        print("[FOTA][srv] verify FAIL:", evt.get("reason"))
    elif ev == "error":
        print("[FOTA][srv] device error:", evt.get("reason"))
    else:
        print("[FOTA][srv] status:", evt)

def _send_next_chunk():
    if not state["active"]:
        return
    off   = state["next_offset"]
    total = state["total"]
    fw    = state["firmware"]
    chunk = state["chunk"]
    if off >= total:
        print("[FOTA][srv] all bytes sent, sending finish")
        publish_cmd({"op": "finish"})
        return

    end = min(off + chunk, total)
    buf = fw[off:end]

    csha = hashlib.sha256(buf).digest()
    obj = {
        "op": "chunk",
        "offset": int(off),
        "data_b64": base64.b64encode(buf).decode("ascii"),
        "sha256_b64": base64.b64encode(csha).decode("ascii")
    }
    publish_cmd(obj)

# Wire MQTT
mqttc.on_connect = on_connect
mqttc.on_message = on_message
mqtt_thr = threading.Thread(target=lambda: mqttc.connect(BROKER_HOST, BROKER_PORT) or mqttc.loop_forever(), daemon=True)
mqtt_thr.start()

# ================= Flask endpoints =================

@app.route("/", methods=["GET"])
def index():
    return (
        "<h1>ESP32 FOTA Server</h1>"
        "<p>POST /fota/start with JSON to begin.</p>"
        "<pre>curl -X POST http://localhost:8080/fota/start -H 'Content-Type: application/json' "
        "-d '{\"firmware_path\":\"C:/path/firmware.bin\",\"version\":\"v1.0.0\",\"chunk\":4096}'</pre>"
        "<p>Other: POST /fota/finish, POST /fota/reboot, POST /fota/ping</p>",
        200,
        {"Content-Type": "text/html"},
    )

@app.route("/fota/ping", methods=["POST"])
def fota_ping():
    publish_cmd({"op": "ping", "ts": int(time.time())})
    Kenc, Kmac = derive_keys(DEV_ID, PSK_HEX)
    print("Kenc=", Kenc.hex().upper())
    print("Kmac=", Kmac.hex().upper())
    st = downlink.get(DEV_ID, {"boot": 0, "seq": 0})
    print("boot_epoch(ms)=", st.get("boot"), "next_seq=", (st.get("seq",0)+1))
    return jsonify({"status":"ping_sent"})

@app.route("/fota/start", methods=["POST"])
def fota_start():
    """
    JSON body:
    {
      "firmware_path": "C:/Users/asus/Downloads/firmware.bin",
      "version": "v1.0.0",
      "chunk": 4096
    }
    """
    data = request.get_json(force=True)
    fw_path = data["firmware_path"]
    version = data.get("version", "unknown")
    chunk   = int(data.get("chunk", DEFAULT_CHUNK))

    # Read firmware
    with open(fw_path, "rb") as f:
        fw = f.read()

    sha_hex = hashlib.sha256(fw).hexdigest().upper()
    nonce = os.urandom(16)

    state.update({
        "firmware": fw,
        "version": version,
        "chunk": chunk,
        "sha_hex": sha_hex,
        "nonce": nonce,
        "next_offset": 0,
        "total": len(fw),
        "active": True
    })

    # Send manifest expected by your firmware
    manifest = {
        "op": "manifest",
        "version": version,
        "size": len(fw),
        "chunk": chunk,
        "sha256_hex": sha_hex,
        "nonce_b64": base64.b64encode(nonce).decode("ascii")
    }
    publish_cmd(manifest)

    return jsonify({
        "status": "manifest_sent",
        "version": version,
        "size": len(fw),
        "sha256_hex": sha_hex,
        "chunk": chunk
    })

@app.route("/fota/finish", methods=["POST"])
def fota_finish():
    publish_cmd({"op": "finish"})
    return jsonify({"status": "finish_sent"})

@app.route("/fota/reboot", methods=["POST"])
def fota_reboot():
    publish_cmd({"op": "reboot"})
    return jsonify({"status": "reboot_sent"})

if __name__ == "__main__":
    # Give MQTT thread a moment to connect before serving HTTP
    time.sleep(0.5)
    app.run(host="0.0.0.0", port=8080, debug=False)
