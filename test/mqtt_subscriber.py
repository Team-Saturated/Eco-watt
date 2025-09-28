#!/usr/bin/env python3
"""
MQTT Subscriber for ECO-WATT Solar Inverter Data
Subscribes to the topic "vdl/replace" and displays incoming data
"""

import paho.mqtt.client as mqtt
import json
import time
from datetime import datetime

# MQTT Configuration (matching server.py)
MQTT_BROKER = "broker.emqx.io"  # Public MQTT broker
MQTT_PORT = 1883
MQTT_TOPIC = "vdl/replace"  # Subscribe to the same topic
MQTT_CLIENT_ID = "ecowatt_subscriber"

def on_connect(client, userdata, flags, rc):
    """Callback when client connects to MQTT broker"""
    if rc == 0:
        print(f"✅ Connected to MQTT broker at {MQTT_BROKER}:{MQTT_PORT}")
        print(f"📡 Subscribing to topic: {MQTT_TOPIC}")
        client.subscribe(MQTT_TOPIC)
        print(f"🔔 Waiting for messages... (Press Ctrl+C to exit)")
        print("-" * 60)
    else:
        print(f"❌ Failed to connect to MQTT broker. Return code {rc}")

def on_message(client, userdata, msg):
    """Callback when a message is received"""
    try:
        # Get current timestamp
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        
        # Decode the message
        payload = msg.payload.decode('utf-8')
        
        print(f"📩 Message received at {timestamp}")
        print(f"📍 Topic: {msg.topic}")
        print(f"📊 Payload size: {len(payload)} bytes")
        
        # Try to parse as JSON
        try:
            data = json.loads(payload)
            print(f"📋 Data type: JSON")
            
            # Display device info
            if 'device_id' in data:
                print(f"🔌 Device ID: {data['device_id']}")
            
            # Display compression stats
            if 'compression_stats' in data:
                stats = data['compression_stats']
                print(f"📈 Compression Stats:")
                print(f"   - Original size: {stats.get('original_bytes', 'N/A')} bytes")
                print(f"   - Compressed size: {stats.get('compressed_bytes', 'N/A')} bytes")
                print(f"   - Compression ratio: {stats.get('ratio', 'N/A')}:1")
                print(f"   - Space saved: {stats.get('space_saved_percent', 'N/A')}%")
            
            # Display solar data summary
            if 'data' in data and isinstance(data['data'], list) and len(data['data']) > 0:
                print(f"🔢 Records count: {len(data['data'])}")
                
                # Show latest reading
                latest = data['data'][-1]
                print(f"🌞 Latest Reading:")
                print(f"   - AC Voltage: {latest.get('ac_voltage', 'N/A')} V")
                print(f"   - AC Current: {latest.get('ac_current', 'N/A')} A")
                print(f"   - AC Power: {latest.get('ac_power', 'N/A')} W")
                print(f"   - Temperature: {latest.get('temperature', 'N/A')} °C")
                print(f"   - Efficiency: {latest.get('efficiency', 'N/A')} %")
                
        except json.JSONDecodeError:
            # Not JSON, display as raw text
            print(f"📋 Data type: Raw text")
            print(f"📄 Raw Content:")
            print(payload[:200] + "..." if len(payload) > 200 else payload)
            
    except Exception as e:
        print(f"❌ Error processing message: {e}")
        print(f"Raw payload: {msg.payload}")
    
    print("-" * 60)

def on_subscribe(client, userdata, mid, granted_qos):
    """Callback when subscription is successful"""
    print(f"✅ Successfully subscribed to {MQTT_TOPIC} (QoS: {granted_qos[0]})")

def on_disconnect(client, userdata, rc):
    """Callback when client disconnects"""
    if rc != 0:
        print(f"⚠️  Unexpected disconnection from MQTT broker (rc: {rc})")
    else:
        print(f"👋 Disconnected from MQTT broker")

def main():
    """Main function to set up and start MQTT subscriber"""
    print("🚀 ECO-WATT MQTT Subscriber Starting...")
    print(f"🌐 Broker: {MQTT_BROKER}:{MQTT_PORT}")
    print(f"📡 Topic: {MQTT_TOPIC}")
    print(f"🆔 Client ID: {MQTT_CLIENT_ID}")
    print("=" * 60)
    
    # Create MQTT client
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1, client_id=MQTT_CLIENT_ID)
    
    # Set callbacks
    client.on_connect = on_connect
    client.on_message = on_message
    client.on_subscribe = on_subscribe
    client.on_disconnect = on_disconnect
    
    try:
        # Connect to broker
        print(f"🔄 Connecting to MQTT broker...")
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        
        # Start the loop to process callbacks
        client.loop_forever()
        
    except KeyboardInterrupt:
        print(f"\n🛑 Interrupted by user")
    except Exception as e:
        print(f"❌ Connection error: {e}")
    finally:
        print(f"🔌 Disconnecting...")
        client.disconnect()
        print(f"✅ MQTT Subscriber stopped")

if __name__ == "__main__":
    main()