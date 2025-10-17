# securelink.py
import os, time, json, hmac, base64, hashlib, struct
from typing import Dict, Any, Optional, Tuple
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from config import DEV_ID, PSK_HEX

HDR_LEN = 1 + 1 + 2 + 8 + 8 + 12
MAC_LEN = 32
downlink: Dict[str, Dict[str, int]] = {}

def hkdf48(ikm: bytes, salt: bytes, info: bytes) -> bytes:
    prk = hmac.new(salt, ikm, hashlib.sha256).digest()
    okm, T, ctr = b"", b"", 0
    while len(okm) < 48:
        ctr += 1
        T = hmac.new(prk, T + info + bytes([ctr]), hashlib.sha256).digest()
        okm += T
    return okm[:48]

def derive_keys(device_id: str, psk_hex: str) -> Tuple[bytes, bytes]:
    salt = hashlib.sha256(device_id.encode()).digest()
    okm = hkdf48(bytes.fromhex(psk_hex), salt, b"EcoWatt")
    return okm[:16], okm[16:48]

def aes_ctr_crypt(Kenc: bytes, iv12: bytes, data: bytes) -> bytes:
    ctr16 = iv12 + b"\x00"*4
    c = Cipher(algorithms.AES(Kenc), modes.CTR(ctr16)).encryptor()
    return c.update(data) + c.finalize()

def seal_downlink(obj: Dict[str, Any], msg_type: int = 2, device_id: str = DEV_ID) -> str:
    Kenc, Kmac = derive_keys(device_id, PSK_HEX)
    st = downlink.get(device_id, {"boot": 0, "seq": 0})
    if not st["boot"]: st["boot"] = int(time.time()*1000)+10_000_000
    st["seq"] += 1; downlink[device_id] = st
    iv12 = os.urandom(12)
    hdr = struct.pack("<BBHQQ", 1, msg_type, 0, st["boot"], st["seq"]) + iv12
    ct = aes_ctr_crypt(Kenc, iv12, json.dumps(obj).encode())
    mac = hmac.new(Kmac, hdr+ct, hashlib.sha256).digest()
    return base64.b64encode(hdr+ct+mac).decode()

def try_open_uplink(sealed_b64: bytes) -> Optional[Dict[str, Any]]:
    """
    Backward-compatible decoder for both sealed and plaintext uplink/ack packets.
    - Works for encrypted AES-CTR + HMAC-SHA256 frames
    - Gracefully accepts unsealed JSON
    """
    # --- Try Base64 decode first ---
    try:
        raw = base64.b64decode(sealed_b64, validate=True)
    except Exception:
        try:
            return json.loads(sealed_b64.decode("utf-8"))
        except Exception:
            return None

    # --- If Base64 actually contained JSON ---
    if raw and raw[0] in (0x7B, 0x5B):  # '{' or '['
        try:
            return json.loads(raw.decode("utf-8"))
        except Exception:
            return None

    # --- Sealed path (HDR + CT + MAC) ---
    if len(raw) < HDR_LEN + MAC_LEN:
        return None

    hdr, ct, mac = raw[:HDR_LEN], raw[HDR_LEN:-MAC_LEN], raw[-MAC_LEN:]
    Kenc, Kmac = derive_keys(DEV_ID, PSK_HEX)

    # Verify integrity
    tag = hmac.new(Kmac, hdr + ct, hashlib.sha256).digest()
    if not hmac.compare_digest(tag, mac):
        print("[SEC] HMAC mismatch")  # temporary debug
        return None

    # Extract header fields for debugging
    ver = hdr[0]
    typ = hdr[1]
    boot = int.from_bytes(hdr[4:12], "little")
    seq  = int.from_bytes(hdr[12:20], "little")
    iv12 = hdr[-12:]

    # AES-CTR decrypt
    plain = aes_ctr_crypt(Kenc, iv12, ct)
    try:
        obj = json.loads(plain.decode("utf-8"))
    except Exception:
        print("[SEC] JSON decode fail")
        return None

    # Attach meta for debugging clarity
    obj["_meta"] = {"ver": ver, "type": typ, "boot": boot, "seq": seq}
    return obj
