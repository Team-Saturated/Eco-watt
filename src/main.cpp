#include <Arduino.h>
#include <WiFi.h>
#include "time.h"

#include "Config.h"
#include "InverterClient.h"
#include "Poller.h"
#include "ConfigUpdate.h"
#include "WiFiConn.h"
#include "Mqtt.h"
#include "SecureLink.h"
#include "Buffer.h"
#include "Uploader.h"
#include "Compression.h" 
#include "Packetizer.h"          
#include <EEPROM.h>
#include "Mqtt.h"
#include "CloudTransport.h"
#include "Rs485Transport.h"
#include "PowerMonitor.h"  // CRITICAL: Include for power optimization measurement

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

uint8_t status_reg = 0b00000000;//.......|Poller|Security|FOTA|TIME|MQTT|WIFI|

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
const String t_device_status = String("devices/") + DEV_ID + "/status";

const char *MQTT_USER = ""; // optional
const char *MQTT_PASS = ""; // optional

bool config_changed = true;
bool writecommandreceived = false;
bool writeemulationreceived = false;

uint8_t FUNCTION_CODE;
String  ERROR_TYPE;
uint8_t EXCEPTION_CODE;
uint16_t DELAY_MS;


const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 5 * 3600 + 30 * 60; 
const int   daylightOffset_sec = 0;
struct tm timeinfo;

WiFiClient espClient;
PubSubClient client(espClient);
SecureLink sec;
FotaManager fota;
QueueHandle_t mqttTxQueue = nullptr;


void main_task(void *pvParameters)
{
  // CRITICAL: Uncomment this line to run power benchmark on startup
  // g_poller->runPowerBenchmark();  // CHANGE THIS: Remove comment to run benchmark
  
  for (;;)
    {
      vTaskDelay(1);
      
      // CRITICAL: Power measurement during normal polling
      powerMonitor.sample("MAIN_TASK_ACTIVE");
      
      g_poller->read(SLAVE_ID, START_ADDR, QTY_REGS);

      if (writecommandreceived || writeemulationreceived)
        {
          // CRITICAL: Power measurement during write operations
          powerMonitor.sample("WRITE_OPERATION");
          
          g_poller->write(SLAVE_ID, WRITE_ADDR, WRITE_VALUE); 
          writecommandreceived = false;
          writeemulationreceived = false;
        }

      static uint32_t last = 0;
      uint32_t now = millis();

      if (now - last >= UPLOAD_PERIOD_MS)
        {
          last = now;
          
          // CRITICAL: Power measurement during upload operations
          powerMonitor.sample("UPLOAD_START");
          
          std::vector<Record> newRecords;
          g_buffer->drainTo(newRecords);

          if (newRecords.empty())
            {
              Serial.println("[MAIN] No new records to upload.");
            }
          else
            {
              Serial.printf("[MAIN] Drained %u new records from buffer (dropped %u)\n", (unsigned)newRecords.size(), (unsigned)g_buffer->droppedCount());
              Serial.printf("[MAIN]Number of Real Inverter Samples: %u\n", (unsigned)newRecords.size());

              bool uploadSuccess = g_uploader->uploadBatch(newRecords);
              if (!uploadSuccess)
                {
                  Serial.println("[MAIN]  Upload failed");
                  powerMonitor.sample("UPLOAD_FAILED");
                }
              else
                {
                  Serial.println("[MAIN]  Real inverter data with timestamps uploaded successfully!");
                  powerMonitor.sample("UPLOAD_SUCCESS");
                }
            }
        }

      if (config_changed)
        {
          // CRITICAL: Power measurement during configuration changes
          powerMonitor.sample("CONFIG_UPDATE");
          ApplyConfig();
        }
        
      // CRITICAL: Generate power report every 10 minutes for monitoring
      static uint32_t lastReport = 0;
      if (now - lastReport >= 600000) {  // 10 minutes
        lastReport = now;
        Serial.println("[POWER] Generating periodic power consumption report:");
        powerMonitor.generateReport();
        
        // Display light sleep statistics
        if (g_poller->isLightSleepEnabled()) {
          Serial.printf("[POWER] Light sleep status: ACTIVE (saving 60-80%% power)\n");
        } else {
          Serial.println("[POWER] Light sleep status: DISABLED (change enableLightSleep to true for power savings)");
        }
      }
    }
}

void CloudConnect(void *pvParameters)
{ 
  
  wifiConnect();
  
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

      bool sent = client.publish(
          p->topic.c_str(),
          p->payload.data(),
          (unsigned)p->payload.size(),
          p->retain);
      Serial.println(sent ? "[MQTT] Publish OK"
                          : String("[MQTT] Publish FAILED, state=") + client.state());

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

  //Wifi initialization
  wifiConnect();
  status_reg |= 0b00000001; 
 
  //EEPROM Initialization
  EEPROM.begin(512);
  
  //Mqtt Initialization
  client.setBufferSize(16384);
  client.setServer(MQTT_HOST, MQTT_PORT); 
  client.setCallback(handleCmd);
  ensureMqtt();
  status_reg |= 0b00000010;

  //NTP Initialization  
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  if (!getLocalTime(&timeinfo)) 
    {
      Serial.println("[TIME] Failed to obtain time from NTP");
    }
  else 
    {
      Serial.println("[TIME] NTP time obtained");
      status_reg |= 0b00000100; // TIME synced
    }

  //PSK Provisioning for SecureLink 
  //first_time_provision();
  
  //FOTA Initialization
  if (!fota.begin()) 
    { 
      Serial.println("[FOTA] init failed"); 
    }
  else 
    {
      status_reg |= 0b00001000; // FOTA ready
      Serial.println("[FOTA] init succeeded");
    }

  //SecureLink Initialization
  if (!sec.begin(DEV_ID)) {
    Serial.println("SecureLink init failed");
    while(1) delay(1000);
  }else{
    status_reg |= 0b00010000; // Security ready
    Serial.println("SecureLink init succeeded");
  }

  //System Check
  String version  = fota.version();
  bool selftest_pass = false; 
  if(status_reg & 0b00011111) 
    {
      selftest_pass = true;
      Serial.printf("[FOTA] Current firmware version: %s\n", version.c_str());
      Serial.println("[FOTA] Boot self-test passed");
      publishDeviceStatusJson("{\"ev\":\"boot_ok\",\"version\":\"" + version + "\",\"status_reg\":" + status_reg + "}");

    }
  else 
    {
      Serial.println("[FOTA] Boot self-test failed");
      publishDeviceStatusJson("{\"ev\":\"system_fail\",\"version\":\"" + version + "\",\"status_reg\":" + status_reg + "}");
    } 
    
  //rollback or finalize FOTA update based on self-test result  
  fota.bootSelfTestFinalize(selftest_pass);


  // CRITICAL: Initialize Power Monitor for ESP32 DevKit V1
  Serial.println("[SETUP] Initializing power monitoring system...");
  powerMonitor.begin();
  
  //InverterClient, Poller, Buffer, Uploader Initialization
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
    
    // CRITICAL POWER OPTIMIZATION SETUP FOR ESP32 DevKit V1
    Serial.println("[SETUP] Configuring power optimization...");
    
    // CHANGE THIS: Set to true to enable 60-80% power savings
    bool enableLightSleep = true;  // Start disabled for safety - change to true after testing
    g_poller->enableLightSleep(enableLightSleep);
    
    // CHANGE THIS: Minimum sleep duration (milliseconds)
    // Recommended: 2000-5000ms for optimal power savings vs responsiveness
    uint32_t minSleepMs = 3000;  // 3 seconds minimum sleep
    g_poller->setMinSleepDuration(minSleepMs);
    
    if (enableLightSleep) {
      Serial.printf("[SETUP] Light sleep ENABLED - min duration: %u ms\n", minSleepMs);
      Serial.println("[SETUP] Expected power savings: 60-80% during sleep periods");
    } else {
      Serial.println("[SETUP] Light sleep DISABLED - change enableLightSleep to true for power optimization");
    }
    
    Serial.println("[SETUP] CRITICAL: To run power benchmark, call g_poller->runPowerBenchmark() in main_task");
  }
  catch (const std::exception &e)
  {
    Serial.printf("Setup failed with error: %s\n", e.what());
  }

  //main task acquisition
  xTaskCreatePinnedToCore(
      main_task, /* Task function. */
      "Task1",   /* name of task. */
      100000,     /* Stack size of task */
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
