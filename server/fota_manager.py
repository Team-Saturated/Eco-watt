# fota_manager.py
import os, time, base64, hashlib
from typing import Dict, Optional
from state import push_log
from mqtt_client import mqttc, publish, state
from config import TOPIC_CMD, DEFAULT_CHUNK
from securelink import seal_downlink, HDR_LEN, MAC_LEN  # for bad_enc tamper

# --- Controls ---
def set_fota_active(active: bool):
    state["active"] = bool(active)

def abort_fota():
    # clear session + tell device (optional "abort" op)
    state.update({"active": False, "firmware": None, "version": None,
                  "sha_hex": None, "nonce": None, "next_offset": 0, "total": 0})
    publish(TOPIC_CMD, {"op": "abort"}, "fota")
    push_log("fota", {"info": "abort_sent"})

def arm_fault(mode: str, once: bool = True, short_len: Optional[int] = None):
    mode = (mode or "").lower()
    assert mode in {"short", "bad_sha", "bad_enc"}
    state["fault"] = {"mode": mode, "once": bool(once), "short_len": short_len or 0}
    push_log("fota", {"info": "fault_armed", "fault": state["fault"]})

# --- Upload flow as before ---
def start_fota_upload(firmware_bytes: bytes, version="v1.0.0", chunk=DEFAULT_CHUNK):
    sha_hex = hashlib.sha256(firmware_bytes).hexdigest().upper()
    nonce = os.urandom(16)
    state.update({
        "firmware": firmware_bytes, "version": version, "chunk": chunk,
        "sha_hex": sha_hex, "nonce": nonce, "next_offset": 0,
        "total": len(firmware_bytes), "active": True, "fault": None
    })
    manifest = {"op": "manifest", "version": version, "size": len(firmware_bytes),
                "chunk": chunk, "sha256_hex": sha_hex,
                "nonce_b64": base64.b64encode(nonce).decode()}
    publish(TOPIC_CMD, manifest, "fota")
    push_log("fota", {"info": "manifest_sent", "size": len(firmware_bytes)})

def _apply_fault_to_buf(buf: bytes) -> (bytes, Optional[bytes], str):
    """Returns (maybe_modified_buf, maybe_bad_sha, fault_mode_used)"""
    f = state.get("fault") or {}
    mode = f.get("mode")
    if not mode:
        return buf, None, ""
    if mode == "short":
        n = int(f.get("short_len") or max(1, len(buf)//2))
        return buf[:max(1, min(n, len(buf)))], None, mode
    if mode == "bad_sha":
        # Intentionally wrong SHA: hash of mutated bytes (xor first byte)
        if not buf:
            return buf, hashlib.sha256(b"").digest(), mode
        corrupted = bytes([buf[0] ^ 0xFF]) + buf[1:]
        return buf, hashlib.sha256(corrupted).digest(), mode
    return buf, None, mode  # bad_enc handled separately

def send_next_chunk():
    if not state["active"]:
        return
    off, total = state["next_offset"], state["total"]
    if off >= total:
        publish(TOPIC_CMD, {"op": "finish"}, "fota")
        push_log("fota", {"info": "finish_sent"})
        return

    end = min(off + state["chunk"], total)
    buf = state["firmware"][off:end]

    # Maybe mutate buffer / sha for short or bad_sha
    buf, bad_sha, mode = _apply_fault_to_buf(buf)

    # Build normal chunk object
    sha = bad_sha or hashlib.sha256(buf).digest()
    obj = {
        "op": "chunk",
        "offset": off,
        "data_b64": base64.b64encode(buf).decode(),
        "sha256_b64": base64.b64encode(sha).decode()
    }

    if mode == "bad_enc":
        # Seal, then flip a byte in ciphertext region so HMAC/dec fails
        sealed_b64 = seal_downlink(obj)
        raw = bytearray(base64.b64decode(sealed_b64))
        # Flip first byte of ciphertext (after header)
        if len(raw) > HDR_LEN + 0:
            raw[HDR_LEN] ^= 0x01
        tampered_b64 = base64.b64encode(bytes(raw)).decode()
        mqttc.publish(TOPIC_CMD, tampered_b64, qos=1)  # bypass helper, send tampered
        push_log("fota", {"dir": "sending from server", "operation": "chunk",
                          "topic": "Fota/Chunk/Sent", "offset": off, "tamper": "bad_enc"})
    else:
        # Normal sealed publish (maybe short or bad_sha)
        publish(TOPIC_CMD, obj, "fota")  # logs already done in helper

    # Clear one-shot fault
    f = state.get("fault")
    if f and f.get("once"):
        state["fault"] = None
