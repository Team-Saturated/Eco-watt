curl -X POST http://localhost:8080/fota/start \
  -H "Content-Type: application/json" \
  -d '{ "firmware_path": "C:/Users/asus/Downloads/firmware.bin", "version": "v1.0.0", "chunk": 4096 }'
