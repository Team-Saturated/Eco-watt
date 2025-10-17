# telemetry.py
import struct
from typing import Any, Dict, List, Optional

# ==========================================================
# REGISTER MAP (address → (name, scale factor, unit))
# ==========================================================
REGISTER_MAP = {
    0: ("Vac1_L1_Phase_voltage", 10, "V"),
    1: ("Iac1_L1_Phase_current", 10, "A"),
    2: ("Fac1_L1_Phase_frequency", 100, "Hz"),
    3: ("Vpv1_PV1_input_voltage", 10, "V"),
    4: ("Vpv2_PV2_input_voltage", 10, "V"),
    5: ("Ipv1_PV1_input_current", 10, "A"),
    6: ("Ipv2_PV2_input_current", 10, "A"),
    7: ("Inverter_internal_temperature", 10, "°C"),
    8: ("Set_export_power_percentage", 1, "%"),
    9: ("Pac_L_Inverter_current_output_power", 1, "W"),
}

# ==========================================================
# Little-endian readers
# ==========================================================
def get16_le(buf: bytes, i: int):
    if i + 2 > len(buf): return None, i
    return struct.unpack_from("<H", buf, i)[0], i + 2

def get32_le(buf: bytes, i: int):
    if i + 4 > len(buf): return None, i
    return struct.unpack_from("<I", buf, i)[0], i + 4

def get64_le(buf: bytes, i: int):
    if i + 8 > len(buf): return None, i
    return struct.unpack_from("<Q", buf, i)[0], i + 8

# ==========================================================
# Variable-length integer (7-bit encoding)
# ==========================================================
def _get_varuint32(buf: bytes, i: int):
    v = 0; shift = 0
    while i < len(buf):
        b = buf[i]; i += 1
        v |= (b & 0x7F) << shift
        if not (b & 0x80):
            return v, i
        shift += 7
        if shift > 28: break
    return None, i

def _unzigzag32(u: int) -> int:
    return (u >> 1) ^ -(u & 1)

def _popcount16(x: int) -> int:
    return bin(x & 0xFFFF).count("1")

# ==========================================================
# Decode helper
# ==========================================================
def decode_register(addr: int, raw: int) -> Dict[str, Any]:
    if addr in REGISTER_MAP:
        name, gain, unit = REGISTER_MAP[addr]
        return {
            "address": addr,
            "name": name,
            "raw_value": raw,
            "value": raw / gain,
            "unit": unit,
        }
    return {"address": addr, "name": f"Unknown_{addr}", "raw_value": raw, "value": raw, "unit": "-"}

# ==========================================================
# Mask + delta decompression (v1)
# ==========================================================
def decompress_delta(hex_string: str) -> Optional[List[Dict[str, Any]]]:
    try:
        buf = bytes.fromhex(hex_string)
    except Exception:
        return None

    if len(buf) < 10:
        return None

    i = 0
    base_ts, i = get64_le(buf, i)
    nrecs, i = get16_le(buf, i)
    if base_ts is None or nrecs is None:
        return None

    last = [0] * 10
    ts = base_ts
    out: List[Dict[str, Any]] = []

    for _ in range(nrecs):
        dt, i = _get_varuint32(buf, i)
        if dt is None: break
        mask, i = get16_le(buf, i)
        if mask is None: break
        mask &= 0x03FF
        ts = (ts + dt) & 0xFFFFFFFFFFFFFFFF
        regs: Dict[str, Any] = {}
        setbits = _popcount16(mask)
        if setbits == 0: continue

        for reg in range(10):
            if not (mask & (1 << reg)): continue
            enc, i = _get_varuint32(buf, i)
            if enc is None: break
            delta = _unzigzag32(enc)
            cur = max(0, min(0xFFFF, last[reg] + delta))
            last[reg] = cur
            d = decode_register(reg, cur)
            regs[d["name"]] = {
                "address": d["address"],
                "raw_value": d["raw_value"],
                "value": d["value"],
                "unit": d["unit"],
            }

        out.append({"timestamp": ts, "register_count": setbits, "registers": regs})

    return out if out else None
