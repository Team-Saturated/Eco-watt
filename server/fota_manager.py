# fota_manager.py
import os, time, base64, hashlib, tempfile
from typing import Dict
from state import push_log
from mqtt_client import mqttc, publish, state
from config import TOPIC_CMD, DEFAULT_CHUNK

def start_fota_upload(firmware_bytes: bytes, version="v1.0.0", chunk=DEFAULT_CHUNK):
    sha_hex = hashlib.sha256(firmware_bytes).hexdigest().upper()
    nonce = os.urandom(16)
    state.update({"firmware": firmware_bytes, "version": version, "chunk": chunk,
                  "sha_hex": sha_hex, "nonce": nonce, "next_offset": 0, "total": len(firmware_bytes), "active": True})
    manifest = {"op": "manifest", "version": version, "size": len(firmware_bytes),
                "chunk": chunk, "sha256_hex": sha_hex,
                "nonce_b64": base64.b64encode(nonce).decode()}
    publish(TOPIC_CMD, manifest, "fota")
    push_log("fota", {"info": "manifest_sent", "size": len(firmware_bytes)})

def send_next_chunk():
    if not state["active"]: return
    off, total = state["next_offset"], state["total"]
    if off >= total:
        publish(TOPIC_CMD, {"op": "finish"}, "fota")
        push_log("fota", {"info": "finish_sent"}); return
    end = min(off + state["chunk"], total)
    buf = state["firmware"][off:end]
    publish(TOPIC_CMD, {
        "op": "chunk", "offset": off,
        "data_b64": base64.b64encode(buf).decode(),
        "sha256_b64": base64.b64encode(hashlib.sha256(buf).digest()).decode()
    }, "fota")
