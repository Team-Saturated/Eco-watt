#include <Arduino.h>
#include <WiFi.h>

#include "Config.h"
#include "InverterClient.h"
#include "Poller.h"
#include "ConfigUpdate.h"
#include "WiFiConn.h"
#include "Mqtt.h"
#include "SecureLink.h"
#include "Acquisition.h"
#include "Buffer.h"
#include "Uploader.h"
#include "Compression.h" 
#include "Packetizer.h"          
#include <EEPROM.h>
#include "Mqtt.h"
#include "CloudTransport.h"
#include "Rs485Transport.h"

#if SIMULATE
CloudTransport *g_transport = nullptr;
#else
Rs485Transport *g_transport = nullptr;
#endif

InverterClient *g_client = nullptr;
Poller *g_poller = nullptr;


RingBuffer *g_buffer = nullptr;
Uploader *g_uploader = nullptr;
std::vector<Record> g_recordBatch;
uint32_t g_batchStartTime = 0;

TaskHandle_t Task1;
TaskHandle_t Task2;

uint16_t POLL_PERIOD_MS = 10000;    
uint16_t UPLOAD_PERIOD_MS = 20000; 
uint16_t BUFFER_CAPACITY = 128;
uint16_t REG_REQ_ID_1 = 0b0000001111111111;
const char *MQTT_HOST = "broker.emqx.io"; 
const uint16_t MQTT_PORT = 1883;

uint16_t WRITE_ADDR = 0x0009;
uint16_t WRITE_VALUE = 0x03FF;

const char *DEV_ID = "esp32-01";
const String t_data = String("devices/") + DEV_ID + "/data/dulmin";
const String t_config = String("devices/") + DEV_ID + "/config";
const String t_config_ack = String("devices/") + DEV_ID + "/ack/config";
const String t_write = String("devices/") + DEV_ID + "/write";
const String t_write_ack = String("devices/") + DEV_ID + "/ack/write";
const String t_fota_cmd = String("devices/") + DEV_ID + "/fota/cmd";
const String t_fota_status = String("devices/") + DEV_ID + "/fota/status";

const char *MQTT_USER = ""; // optional
const char *MQTT_PASS = ""; // optional

bool config_changed = false;
bool writecommandreceived = false;


WiFiClient espClient;
PubSubClient client(espClient);
SecureLink sec;
FotaManager fota;
QueueHandle_t mqttTxQueue = nullptr;


void main_task(void *pvParameters)
{
  for (;;)
  {
    
    vTaskDelay(1);
    g_poller->read(SLAVE_ID, START_ADDR, QTY_REGS);

    if (writecommandreceived)
    {
      g_poller->write(SLAVE_ID, WRITE_ADDR, WRITE_VALUE); // Example value to write
      writecommandreceived = false;
    }

    static uint32_t last = 0;
    uint32_t now = millis();

    if (now - last >= UPLOAD_PERIOD_MS)
    {
      last = now;
      std::vector<Record> newRecords;
      g_buffer->drainTo(newRecords);

      if (newRecords.empty())
      {
        Serial.println("[MAIN] No new records to upload.");
      }
      else
      {
        Serial.printf("[MAIN] Printing %u new records:\n", (unsigned)newRecords.size());
        for (size_t i = 0; i < newRecords.size(); i++) {
          Serial.printf("Record %u: timestamp=%u, start address=%u, qty=%u, rawFrameHex=%s\n", 
                        (unsigned)i, 
                        newRecords[i].ts_ms,
                        newRecords[i].raw.data(),
                        newRecords[i].raw.size());  
        }
        Serial.printf("[MAIN] Drained %u new records from buffer (dropped %u)\n", (unsigned)newRecords.size(), (unsigned)g_buffer->droppedCount());
        Serial.printf("Number of Real Inverter Samples: %u\n", (unsigned)newRecords.size());
        uint32_t original_size = newRecords.size() * sizeof(Record);
        Serial.printf("Original Payload Size: %u bytes\n", original_size);

        bool uploadSuccess = g_uploader->uploadBatch(newRecords);
        if (!uploadSuccess)
        {
          Serial.println("[MAIN]  Upload failed");
        }
        else
        {
          Serial.println("[MAIN]  Real inverter data with timestamps uploaded successfully!");
        }
      }
    }

    if (config_changed)
    {

      ApplyConfig();
      
    } 
  }
}

void CloudConnect(void *pvParameters)
{ 
  
  wifiConnect();
  client.setServer(MQTT_HOST, MQTT_PORT);
    
  client.setCallback(handleCmd);
  
  uint32_t last = 0;
  uint32_t now = 0;
  for (;;)
  {
    vTaskDelay(1);
    client.loop();
    now = millis();
    if(now-last >= CONNECTION_CHECK_PERIOD_MS)
    {
        last = now;
      if (!checkConnection())
      {
        wifiConnect();
      }

      if (!client.connected())
      {
        ensureMqtt();
      }
      
    }

    MqttTx* p = nullptr;
    if (xQueueReceive(mqttTxQueue, &p, pdMS_TO_TICKS(5)) == pdPASS && p) {
      
      std::vector<uint8_t> cipher;
      Serial.println("[MQTT] Publishing message to topic: " + p->topic);
      bool ok = encryptPayload(p->payload.data(), p->payload.size(), cipher);
      Serial.println(ok ? "[MQTT] Encryption successful" : "[MQTT] Encryption failed");
      
      if (ok && client.connected()) {
        Serial.printf("[MQTT] Publishing %u  to topic %s\n", (unsigned)cipher.data(), p->topic.c_str());
        (void)client.publish(p->topic.c_str(), cipher.data(), cipher.size(), p->retain);
      }
      delete p;
    }
  }

}

void first_time_provision() {
  uint8_t myPSK[32] = {
    0x49, 0x68, 0xA7, 0xE8, 0x83, 0x5B, 0xC6, 0xEC,
    0x5B, 0xDB, 0xE1, 0x5A, 0xA9, 0xE7, 0xC4, 0x78,
    0xE5, 0x61, 0x6E, 0x33, 0xAA, 0x0C, 0xC4, 0xCA,
    0xDB, 0x53, 0xA8, 0x1A, 0xA2, 0x0F, 0xA7, 0x27
  };
  sec.provisionPSK(myPSK);
}

void setup()
{
  Serial.begin(115200);
  mqttTxQueue = xQueueCreate(32, sizeof(MqttTx*));
  delay(200);
  wifiConnect();
  client.setBufferSize(16384);
  EEPROM.begin(512);
  
  //first_time_provision(); 
  if (!fota.begin()) { Serial.println("[FOTA] init failed"); }
  if (!sec.begin(DEV_ID)) {
    Serial.println("SecureLink init failed");
    while(1) delay(1000);
  }
  bool selftest_pass = true; 
  fota.bootSelfTestFinalize(selftest_pass);
  if (selftest_pass) {
    publishFotaJsonACK("{\"ev\":\"boot_ok\",\"version\":\"(fill from NVS or compile-time)\"}");
  } else {
    // If failing here, bootloader will roll back automatically
    // You can still try to publish, but reboot happens quickly.
  }
  try
  {
    #if SIMULATE
      g_transport = new CloudTransport(String(API_READ_URL), String(API_WRITE_URL), String(AUTH_HEADER), REQ_TIMEOUT_MS);
    #else
      g_transport = new Rs485Transport(RS485_SERIAL, RS485_BAUD, RS485_DE_RE_PIN, REQ_TIMEOUT_MS);
    #endif

    g_client = new InverterClient(*g_transport);  
    g_buffer = new RingBuffer(BUFFER_CAPACITY);
    g_uploader = new Uploader(String(API_UPLOAD_URL), String(AUTH_HEADER));
    g_poller = new Poller(*g_client, POLL_PERIOD_MS, *g_buffer);
  }
  catch (const std::exception &e)
  {
    Serial.printf("Setup failed with error: %s\n", e.what());
  }

  //main task acquisition
  xTaskCreatePinnedToCore(
      main_task, /* Task function. */
      "Task1",   /* name of task. */
      40000,     /* Stack size of task */
      NULL,      /* parameter of the task */
      1,         /* priority of the task */
      &Task1,    /* Task handle to keep track of created task */
      1);        /* pin task to core 1 */
  delay(500);

  // connectivity task
  xTaskCreatePinnedToCore(
      CloudConnect, /* Task function. */
      "Task2",      /* name of task. */
      40000,        /* Stack size of task */
      NULL,         /* parameter of the task */
      1,            /* priority of the task */
      &Task2,       /* Task handle to keep track of created task */
      0);           /* pin task to core 0 */
  delay(500);
  
}

void loop()
{
}
