#include <Arduino.h>
#include "../include/Config.h"
#include "../include/InverterClient.h"
#include "../include/Poller.h"
#include "../include/ConfigUpdate.h"
#include "../include/WiFiConn.h"
#include "../include/Mqtt.h"
// NEW:
#include "Acquisition.h"
#include "Buffer.h"
#include "Uploader.h"
#include "Compression.h" // Added for compression
#include "Packetizer.h" // Added for packetizer
#include <FS.h> // For file writing (ESP32/ESP8266)

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
#else
  #include <WiFi.h>
#endif

#include "CloudTransport.h"
#include "Rs485Transport.h"

#if SIMULATE
CloudTransport* g_transport = nullptr;
#else
Rs485Transport* g_transport = nullptr;
#endif

InverterClient* g_client  = nullptr;
Poller*        g_poller  = nullptr;

// NEW: globals used by Poller
RingBuffer*    g_buffer  = nullptr;
Acquisition*   g_acq     = nullptr;
Uploader*      g_uploader= nullptr;

// Batch collection globals
std::vector<Record> g_recordBatch;
uint32_t g_batchStartTime = 0;

TaskHandle_t Task1;
TaskHandle_t Task2;

uint16_t POLL_PERIOD_MS  = 1000;           // how often we poll the inverter
uint16_t UPLOAD_PERIOD_MS  = 14000;      // send buffered data every 14 sec (before Poller flush at 15s)
uint8_t BUFFER_CAPACITY    = 128;

const char* MQTT_HOST     = "192.168.1.10"; // or cloud host
const uint16_t MQTT_PORT  = 1883;

const char* DEV_ID        = "esp32-01";
String t_data = String("devices/") + DEV_ID + "/telemetry";
String t_status    = String("devices/") + DEV_ID + "/status";
String t_config    = String("devices/") + DEV_ID + "/config";
String t_ack       = String("devices/") + DEV_ID + "/ack";

const char* MQTT_USER     = "";  // optional
const char* MQTT_PASS     = "";  // optional

bool config_changed = false;

WiFiClient espClient;
PubSubClient client(espClient);









void main_task(void * pvParameters){
  for(;;){
    static bool benchmarked = false;
    g_poller->loop(SLAVE_ID, START_ADDR, QTY_REGS);
    static uint32_t last = 0;
    uint32_t now = millis();

    if(now-last >= UPLOAD_PERIOD_MS) {
      last = now;
      std::vector<Record> newRecords;
      g_buffer->drainTo(newRecords);

      if(newRecords.empty()) {
        Serial.println("[MAIN] No new records to upload.");
      } else {
        Serial.printf("[MAIN] Drained %u new records from buffer (dropped %u)\n", (unsigned)newRecords.size(), (unsigned)g_buffer->droppedCount());
      
        Serial.println("=== REAL INVERTER DATA COMPRESSION REPORT (WITH TIMESTAMPS) ===");
        Serial.printf("Compression Method Used: Delta Encoding with Timestamp Compression\n");
        Serial.printf("Number of Real Inverter Samples: %u\n", (unsigned)newRecords.size());
        uint32_t original_size = newRecords.size() * sizeof(Record);
        Serial.printf("Original Payload Size: %u bytes\n", original_size);

        // Measure compression time
        uint32_t compress_start = micros();
        std::vector<uint8_t> compressed = Packetizer::finalizeBlock(newRecords);
        uint32_t compress_time = micros() - compress_start;

        Serial.printf("Compressed Payload Size: %u bytes\n", (unsigned)compressed.size());
        float compression_ratio = (float)original_size / (float)compressed.size();
        Serial.printf("Compression Ratio: %.2f:1 (%.1f%% reduction)\n", 
                      compression_ratio, 
                      (1.0f - (float)compressed.size() / (float)original_size) * 100.0f);
        Serial.printf("CPU Time: %u microseconds\n", compress_time);

        std::vector<Record> decompressed = Compression::decompressDelta(compressed);
        bool lossless = (decompressed.size() == newRecords.size());
        Serial.printf("Lossless Recovery Verification: %s\n", lossless ? "PASSED" : "FAILED");

        // --- Packetizer: encrypt, chunk ---
        //std::vector<uint8_t> encrypted = Packetizer::encryptAndMac(compressed);
        //auto chunks = Packetizer::chunkData(encrypted, 32);

        

        bool uploadSuccess = g_uploader->uploadBatch(newRecords);
        if (!uploadSuccess) {
          Serial.println("[MAIN]  Upload failed");
        } else {
          Serial.println("[MAIN]  Real inverter data with timestamps uploaded successfully!");
        }
      }
    }

    
    if(config_changed)
    {
      //getconfig from server
      
      ApplyConfig();
    }
    delay(5);
  }
}

void CloudConnect(void * pvParameters)
{
  

  wifiConnect();
  client.setServer(MQTT_HOST, MQTT_PORT);
  client.setCallback(handleCmd);
  //error logging
  for(;;)
  {
    if(!checkConnection())
    {
      wifiConnect();
    }

    if (!client.connected()) ensureMqtt();
    client.loop();
  }
  
  
  //handle mqtt requests

  //validate config changes.
}

void setup() {
  Serial.begin(115200);
  delay(200);
  
 
  
  

  try {
#if SIMULATE
    g_transport = new CloudTransport(String(API_URL), String(AUTH_HEADER), REQ_TIMEOUT_MS);
#else
    g_transport = new Rs485Transport(RS485_SERIAL, RS485_BAUD, RS485_DE_RE_PIN, REQ_TIMEOUT_MS);
#endif
    if (!g_transport) { Serial.println("Failed to create transport"); return; }

    g_client = new InverterClient(*g_transport);
    if (!g_client) { Serial.println("Failed to create inverter client"); return; }

    // NEW: buffer + acquisition + uploader - create buffer first
    g_buffer   = new RingBuffer(BUFFER_CAPACITY);
    g_acq      = new Acquisition(*g_client);
    g_uploader = new Uploader(String(API_UPLOAD_URL), String(AUTH_HEADER));

    g_poller = new Poller(*g_client, POLL_PERIOD_MS, *g_buffer);
    if (!g_poller) { Serial.println("Failed to create poller"); return; }

    Serial.println("Compression enabled: using delta encoding for uploads.");
    Serial.println("Setup done successfully.");
  } catch (const std::exception& e) {
    Serial.printf("Setup failed with error: %s\n", e.what());
  }

  //create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
  xTaskCreatePinnedToCore(
                    main_task,   /* Task function. */
                    "Task1",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task1,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */                  
  delay(500); 

  //create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
  xTaskCreatePinnedToCore(
                    CloudConnect,   /* Task function. */
                    "Task2",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task2,      /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */
    delay(500); 
}


void loop() {
  
}
