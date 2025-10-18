# config.py
BROKER_HOST = "broker.emqx.io"
BROKER_PORT = 1883
MQTT_USER   = None
MQTT_PASS   = None

DEV_ID      = "esp32-01"

# Topics
TOPIC_CMD     = f"devices/{DEV_ID}/fota/cmd"
TOPIC_STAT    = f"devices/{DEV_ID}/fota/status"
TOPIC_DATA    = f"devices/{DEV_ID}/data/dulmin"
TOPIC_CONFIG  = f"devices/{DEV_ID}/config"
TOPIC_WRITE   = f"devices/{DEV_ID}/write"
TOPIC_ACK_FOTA   = f"devices/{DEV_ID}/fota/status"
TOPIC_ACK_CONFIG = f"devices/{DEV_ID}/ack/config"
TOPIC_ACK_WRITE  = f"devices/{DEV_ID}/ack/write"
TOPIC_DEVICE_STATUS = f"devices/{DEV_ID}/status"

# Security
PSK_HEX = "4968A7E8835BC6EC5BDBE15AA9E7C478E5616E33AA0CC4CADB53A81AA20FA727"

DEFAULT_CHUNK = 4096
MAX_EVENTS    = 200
MAX_RECORDS   = 1000

ERROR_FLAG_API_URL = "http://20.15.114.131:8080/api/user/error-flag/add"
ERROR_FLAG_API_KEY =  "NjhhZWIwNDU1ZDdmMzg3MzNiMTQ5YjhmOjY4YWViMDQ1NWQ3ZjM4NzMzYjE0OWI4NQ=="