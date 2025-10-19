# mqtt_client.py
import threading, time, hashlib, base64, json
import paho.mqtt.client as mqtt
from config import *
from securelink import seal_downlink, try_open_uplink
from state import push_log, data_records, trim_records
from telemetry import decompress_delta

mqttc = mqtt.Client()
if MQTT_USER and MQTT_PASS:
    mqttc.username_pw_set(MQTT_USER, MQTT_PASS)

state = {"firmware": None, "version": None, "chunk": DEFAULT_CHUNK,
         "sha_hex": None, "nonce": None, "next_offset": 0, "total": 0, "active": False}

def publish(topic: str, obj: dict, bucket: str):
    payload = seal_downlink(obj)
    mqttc.publish(topic, payload, qos=1)
    push_log(bucket, {"dir": "sending from server", "offset": obj.get('offset'),"operation": obj.get('op'),"topic":"Fota/Chunk/Sent"})

def on_connect(client, userdata, flags, rc):
    subs = [(TOPIC_STAT,1),(TOPIC_DATA,1),(TOPIC_ACK_FOTA,1),
            (TOPIC_ACK_CONFIG,1),(TOPIC_ACK_WRITE,1),(TOPIC_DEVICE_STATUS,1)]
    client.subscribe(subs)
    print(f"[MQTT] Connected rc={rc}")

def on_message(client, userdata, msg):
    topic = msg.topic
    obj = try_open_uplink(msg.payload)
    if not obj:
        push_log("data", {"topic": topic, "error": "parse_failed"})
        return

    if topic == TOPIC_STAT:
        push_log("fota", {"request_offset": obj.get("next_offset"), "event": obj.get("ev"),  "meta": obj.get("meta"), "topic":"Fota/Chunk/Request"})
        ev = obj.get("ev")
        if ev in ("need_chunks", "progress"):
            from fota_manager import send_next_chunk, state
            state["next_offset"] = int(obj.get("next_offset", 0))
            state["total"] = int(obj.get("total", 0))
            send_next_chunk()
        elif ev == "finish":
            push_log("fota", {"info": "device reported finish"})
        return

    # --- DATA (uplink) ---
    if topic == TOPIC_DATA:
        push_log("data", {"topic": "data/rx", "info": "rx"})
        if "payload_hex" in obj:
            from telemetry import decompress_delta
            recs = decompress_delta(obj["payload_hex"])
            
            if recs:
                import time
                for r in recs:
                    r["server_ts"] = int(time.time() * 1000)
                from state import data_records, trim_records
                data_records.extend(recs)
                trim_records()
                push_log("data", {"topic": "data/decoded", "decoded": len(recs)})
        return
    if topic == TOPIC_ACK_WRITE:
        
        if obj:
            push_log("write", {"topic": "ack/write", "ack": obj.get('error')})
            print(f"[MQTT] Write ACK: {obj.get('error')}")
        else:
            push_log("write", {"topic": "ack/write", "error": "parse_failed"})
        return
    if topic == TOPIC_ACK_CONFIG:
        
        push_log("config", {"topic": "config/ack", "ack": obj or {"error":"parse_failed"}})
        return
    if topic == TOPIC_ACK_FOTA:
        
        push_log("fota", {"topic": "ack/fota", "ack": obj or {"error":"parse_failed"}})
        return
    if topic == TOPIC_DEVICE_STATUS:
        status = ""
        try :
            reg = obj.get("status_reg", 0)

            flags = []

            if reg & 0b00001:
                flags.append("WIFI connected")
            if reg & 0b00010:
                flags.append("MQTT connected")
            if reg & 0b00100:
                flags.append("TIME synced")
            if reg & 0b01000:
                flags.append("FOTA ok")
            if reg & 0b10000:
                flags.append("Security ok")

            status = " | ".join(flags) if flags else f"Unknown ({reg})"
            push_log("device", {"topic": "device/status", "status": status})

        except Exception as e:
            push_log("device", {"topic": "device/status", "error": str(e)})
            return
        #push_log("device", {"topic": "device/status", "status": obj or {"error":"parse_failed"}})
        print(f"[MQTT] Device Status: {obj}")
        return
    

def publish_config(obj: dict):
    """Send sealed configuration JSON to the device."""

    payload = seal_downlink(obj)
    mqttc.publish(TOPIC_CONFIG, payload, qos=1, retain=False)
    #push_log("config", {"dir": "tx", "sent": obj})
    print(f"[MQTT] Sent CONFIG: {obj}")

def publish_write(topic: str, obj: dict, bucket: str):
    print(f"[MQTT] Sending WRITE: {obj}")
    payload = seal_downlink(obj)
    mqttc.publish(topic, payload, qos=1)
    push_log(bucket, {"dir": "sending from server","operation": obj.get('op')})


mqttc.on_connect = on_connect
mqttc.on_message = on_message

def start_mqtt():
    t = threading.Thread(target=lambda: mqttc.connect(BROKER_HOST,BROKER_PORT) or mqttc.loop_forever(), daemon=True)
    t.start()
