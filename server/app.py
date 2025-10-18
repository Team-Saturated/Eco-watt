#app.py
from flask import Flask, jsonify, request, send_from_directory
import time, hashlib, base64
from mqtt_client import start_mqtt, publish
from fota_manager import start_fota_upload
from state import logs, data_records, push_log
from config import *

app = Flask(__name__, static_folder="web/static")

@app.route("/")
def index():
    return send_from_directory("web", "index.html")

@app.route("/api/logs/<kind>")
def api_logs(kind):
    return jsonify({"events": logs.get(kind, [])})

@app.route("/api/data")
def api_data():
    limit = int(request.args.get("limit", "100"))
    return jsonify({"records": data_records[-limit:]})

@app.route("/api/fota/upload", methods=["POST"])
def api_fota():
    if "firmware" not in request.files: return jsonify({"error":"no_firmware"}),400
    f = request.files["firmware"]
    data = f.read()
    version = request.form.get("version","v1.0.0")
    chunk = int(request.form.get("chunk",str(DEFAULT_CHUNK)))
    start_fota_upload(data, version, chunk)
    return jsonify({"ok":True,"version":version,"size":len(data)})

@app.route("/api/reboot", methods=["POST"])
def api_reboot():
    publish(TOPIC_CMD, {"op":"reboot"}, "fota")
    push_log("fota", {"info":"reboot_sent"})
    return jsonify({"ok":True})


@app.route("/api/write", methods=["POST"])
def api_write():
    try:
        body = request.get_json(force=True) or {}
    except Exception:
        return jsonify({"ok": False, "error": "bad_json"}), 400

    if "address" not in body or "value" not in body:
        return jsonify({"ok": False, "error": "need_address_and_value"}), 400

    try:
        addr = int(body["address"])
        val  = int(body["value"])
    except Exception:
        return jsonify({"ok": False, "error": "address_value_must_be_int"}), 400

    payload = {"op": "write", "address": addr, "value": val}
    from mqtt_client import publish_write
    publish_write(TOPIC_WRITE, payload, "write")
    from state import push_log
    push_log("write", {"dir": "tx", "sent": payload,"topic":'Write/Command'})
    return jsonify({"ok": True, "sent": payload})

# ---- CONFIG: get & send ----
current_config = {
    "poll_period_ms": 10000,
    "upload_period_ms": 20000,
    "buffer_capacity": 256,
    "reg_req_id_1": 1023  # all 10 regs by default
}

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
    for k, v in patch.items():
        if k in allowed and isinstance(v, (int, float)):
            current_config[k] = int(v)

    from mqtt_client import publish_config
    publish_config(current_config.copy())

    from state import push_log
    push_log("config", {"topic": "config/sent", "dir": "tx", "sent": current_config.copy()})

    return jsonify({"ok": True, "sent": current_config})

@app.route("/api/error-flag/add", methods=["POST"])
def api_error_flag_add():
    try:
        body = request.get_json(force=True) or {}
    except Exception:
        return jsonify({"ok": False, "error": "bad_json"}), 400

    t = (body.get("errorType") or "").upper()
    allowed = {"EXCEPTION", "CRC_ERROR", "CORRUPT", "PACKET_DROP", "DELAY"}
    if t not in allowed:
        return jsonify({"ok": False, "error": "invalid_errorType"}), 400

    if t == "EXCEPTION" and "exceptionCode" not in body:
        return jsonify({"ok": False, "error": "exceptionCode_required"}), 400
    if t == "DELAY" and "delayMs" not in body:
        return jsonify({"ok": False, "error": "delayMs_required"}), 400

    import requests
    from config import ERROR_FLAG_API_URL, ERROR_FLAG_API_KEY
    headers = {
        "accept": "*/*",
        "Authorization": ERROR_FLAG_API_KEY,
        "Content-Type": "application/json",
    }

    try:
        resp = requests.post(ERROR_FLAG_API_URL, headers=headers, json=body, timeout=10)
    except requests.RequestException as e:
        return jsonify({"ok": False, "error": f"upstream_error: {e}"}), 502

    if resp.status_code != 200:
        return jsonify({
            "ok": False,
            "upstream_status": resp.status_code,
            "upstream_text": resp.text[:500]
        }), 502

    # optional: log to your existing log bucket
    try:
        from state import push_log
        push_log("device", {"topic": "error-flag/add", "dir": "tx", "forwarded": body})
    except Exception:
        pass

    return jsonify({"ok": True, "forwarded": body})


if __name__ == "__main__":
    start_mqtt()
    app.run(host="0.0.0.0", port=8080)
