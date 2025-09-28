#!/usr/bin/env python3
"""
MQTT Test Publisher for ECO-WATT
Manually publish test messages to the topic "vdl/replace"
"""

import paho.mqtt.client as mqtt
import json
import time
from datetime import datetime

# MQTT Configuration (matching server.py)
MQTT_BROKER = "broker.emqx.io"  # Public MQTT broker
MQTT_PORT = 1883
MQTT_TOPIC = "vdl/replace"  # Publish to the same topic
MQTT_CLIENT_ID = "ecowatt_test_publisher"

def on_connect(client, userdata, flags, rc):
    """Callback when client connects to MQTT broker"""
    if rc == 0:
        print(f"✅ Connected to MQTT broker at {MQTT_BROKER}:{MQTT_PORT}")
    else:
        print(f"❌ Failed to connect to MQTT broker. Return code {rc}")

def on_publish(client, userdata, mid):
    """Callback when message is published"""
    print(f"✅ Message published successfully (mid: {mid})")

def create_test_data():
    """Create sample solar inverter data for testing"""
    current_time = int(time.time())
    
    # Sample solar inverter data (10 registers as per Config.h)
    sample_data = []
    for i in range(15):  # 15 records
        sample_data.append({
            "timestamp": current_time + i,
            "ac_voltage": 230.5 + (i * 0.1),
            "ac_current": 4.2 + (i * 0.05),
            "ac_power": 968.1 + (i * 2.0),
            "ac_frequency": 50.0,
            "dc_voltage": 350.8 + (i * 0.2),
            "dc_current": 2.8 + (i * 0.03),
            "dc_power": 982.2 + (i * 1.8),
            "temperature": 42.5 + (i * 0.1),
            "status": 1,
            "efficiency": 98.6 - (i * 0.05)
        })
    
    # Format like server.py output
    payload = {
        "timestamp": datetime.now().isoformat(),
        "device_id": "ESP32_TEST_DEVICE",
        "data": sample_data,
        "compression_stats": {
            "original_bytes": 2880,
            "compressed_bytes": 314,
            "ratio": 9.17,
            "space_saved_percent": 89.1
        },
        "metadata": {
            "upload_interval": "15 seconds",
            "records_per_upload": len(sample_data),
            "total_registers": 10
        }
    }
    
    return json.dumps(payload)

def main():
    """Main function to publish test messages"""
    print("🚀 ECO-WATT MQTT Test Publisher Starting...")
    print(f"🌐 Broker: {MQTT_BROKER}:{MQTT_PORT}")
    print(f"📡 Topic: {MQTT_TOPIC}")
    print(f"🆔 Client ID: {MQTT_CLIENT_ID}")
    print("=" * 60)
    
    # Create MQTT client
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1, client_id=MQTT_CLIENT_ID)
    
    # Set callbacks
    client.on_connect = on_connect
    client.on_publish = on_publish
    
    try:
        # Connect to broker
        print(f"🔄 Connecting to MQTT broker...")
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_start()
        
        # Wait for connection
        time.sleep(2)
        
        # Publish test messages
        print(f"📤 Publishing test message...")
        
        # Create test data
        test_payload = create_test_data()
        
        # Publish message
        result = client.publish(MQTT_TOPIC, test_payload)
        
        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            print(f"✅ Message queued for publishing")
            print(f"📊 Payload size: {len(test_payload)} bytes")
            print(f"🔢 Records: 15 solar inverter readings")
            print(f"📈 Simulated compression: 9.17:1 ratio")
        else:
            print(f"❌ Failed to queue message for publishing (rc: {result.rc})")
        
        # Wait for message to be published
        print(f"\n⏳ Waiting for message to be published...")
        time.sleep(3)
        
    except KeyboardInterrupt:
        print(f"\n🛑 Interrupted by user")
    except Exception as e:
        print(f"❌ Error: {e}")
    finally:
        print(f"🔌 Disconnecting...")
        client.loop_stop()
        client.disconnect()
        print(f"✅ Test Publisher stopped")

if __name__ == "__main__":
    main()