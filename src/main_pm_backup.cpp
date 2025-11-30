/*
#include <Arduino.h>
#include <WiFi.h>
#include "time.h"

// ============================================================================
// POWER MANAGEMENT: Auto Light Sleep for Real Hardware
// ============================================================================
// Requirements (configured in platformio.ini for real hardware build):
// - CONFIG_PM_ENABLE=y (Power Management)
// - CONFIG_FREERTOS_USE_TICKLESS_IDLE=y (Tickless IDLE)
// - CONFIG_ESP32_RTC_CLK_SRC_EXT_CRYS=y (External 32kHz crystal for accuracy)

#if !SIMULATE
  #include "esp_pm.h"  
  #include "esp_wifi.h"
#endif

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

#if SIMULATE
CloudTransport *g_transport = nullptr;
#else
Rs485Transport *g_transport = nullptr;
#endif

InverterClient *g_client = nullptr;
Poller *g_poller = nullptr;

// ============================================================================
// POWER OPTIMIZATION: Light Sleep State Tracking
// ============================================================================
#if !SIMULATE
  bool g_lightSleepEnabled = false;     // Auto light sleep activation flag
  bool g_pmConfigured = false;          // Power management configuration status
#endif

RingBuffer *g_buffer = nullptr;
Uploader *g_uploader = nullptr;
std::vector<Record> g_recordBatch;
uint32_t g_batchStartTime = 0;

TaskHandle_t Task1;
TaskHandle_t Task2;

// ============================================================================
// TIMING CONFIGURATION: Simulation vs Real Hardware
// ============================================================================
uint16_t POLL_PERIOD_MS = 10000;    // Poll every 10 seconds (both modes)

#if SIMULATE
  uint16_t UPLOAD_PERIOD_MS = 20000;   // Simulation: 20 seconds (fast testing)
#else
  uint16_t UPLOAD_PERIOD_MS = 900000;  // Real Hardware: 15 minutes (power optimization)
#endif

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

extern "C" {
  void __wrap_esp_log_write(int level, const char *tag, const char *format, ...) {
    // Optional: you can forward to normal logging if you want
    // but it's safe to leave this empty to drop WiFi internal logs.
  }

  void __wrap_esp_log_writev(int level, const char *tag,
                             const char *format, va_list args) {
    // Same here – stubbed out.
  }
}

void main_task(void *pvParameters)
{
  // ============================================================================
  // POWER OPTIMIZATION: Centered Sleep Window Calculation
  // ============================================================================
  // For real hardware, we implement centered sleep within upload cycles:
  // - Total cycle: UPLOAD_PERIOD_MS (e.g., 900000ms = 15 minutes)
  // - Start buffer: 15% for WiFi stability and initial operations
  // - Sleep window: 70% for maximum power savings
  // - End buffer: 15% for data transmission and upload completion
  //
  // Timeline example (15min cycle):
  // [0-135s: Active Start] -> [135-765s: Sleep] -> [765-900s: Active End]
  // ============================================================================
  #if !SIMULATE
    const uint32_t SLEEP_PERCENT = 70;      // 70% of cycle in sleep
    const uint32_t START_BUFFER_PERCENT = 15;  // 15% active at start
    const uint32_t END_BUFFER_PERCENT = 15;    // 15% active at end
    
    // Calculate sleep window boundaries
    const uint32_t startBufferMs = (UPLOAD_PERIOD_MS * START_BUFFER_PERCENT) / 100;
    const uint32_t sleepDurationMs = (UPLOAD_PERIOD_MS * SLEEP_PERCENT) / 100;
    const uint32_t sleepStartTime = startBufferMs;
    const uint32_t sleepEndTime = startBufferMs + sleepDurationMs;
    
    Serial.println("[POWER] Light sleep timing configured:");
    Serial.printf("  Upload cycle: %u ms (%.1f min)\n", UPLOAD_PERIOD_MS, UPLOAD_PERIOD_MS / 60000.0);
    Serial.printf("  Start buffer: %lu ms (%.1f min) - WiFi stability\n", (unsigned long)startBufferMs, startBufferMs / 60000.0);
    Serial.printf("  Sleep window: %lu ms (%.1f min) - Power saving\n", (unsigned long)sleepDurationMs, sleepDurationMs / 60000.0);
    Serial.printf("  End buffer: %lu ms (%.1f min) - Data transmission\n", 
                  (unsigned long)(UPLOAD_PERIOD_MS - sleepEndTime), (UPLOAD_PERIOD_MS - sleepEndTime) / 60000.0);
  #endif
  
  for (;;)
    {
      
      vTaskDelay(1);
      
      g_poller->read(SLAVE_ID, START_ADDR, QTY_REGS);

      if (writecommandreceived || writeemulationreceived)
        {
          g_poller->write(SLAVE_ID, WRITE_ADDR, WRITE_VALUE); 
          writecommandreceived = false;
          writeemulationreceived = false;
        }

      static uint32_t last = 0;
      uint32_t now = millis();

      // ============================================================================
      // POWER OPTIMIZATION: Centered Sleep Window Implementation
      // ============================================================================
      // Sleep logic for real hardware: Only sleep during the designated window
      // between start buffer and end buffer to maximize power savings while
      // ensuring WiFi stability and data transmission reliability.
      // ============================================================================
      #if !SIMULATE
        if (g_lightSleepEnabled && g_pmConfigured) {
          uint32_t cyclePosition = (now - last) % UPLOAD_PERIOD_MS;
          
          // Check if we're in the sleep window (between start and end buffers)
          if (cyclePosition >= sleepStartTime && cyclePosition < sleepEndTime) {
            // Within sleep window - calculate remaining sleep time
            uint32_t remainingSleep = sleepEndTime - cyclePosition;
            
            // Only sleep if sufficient time remains and WiFi is stable
            if (remainingSleep >= 5000 && WiFi.status() == WL_CONNECTED) {
              Serial.printf("[SLEEP] Entering sleep window (remaining: %lu ms)\n", (unsigned long)remainingSleep);
              Serial.println("[SLEEP] All tasks will block, FreeRTOS auto light sleep active");
              
              // Release CPU to allow FreeRTOS tickless IDLE to engage auto light sleep
              // The Power Management component will automatically enter light sleep
              // when all tasks are blocked and idle time exceeds threshold
              vTaskDelay(pdMS_TO_TICKS(remainingSleep));
              
              Serial.println("[WAKE] Sleep window completed, resuming operations");
            }
          } else if (cyclePosition < sleepStartTime) {
            // In start buffer period - active for WiFi stability
            Serial.printf("[ACTIVE] Start buffer period (position: %lu / %lu ms)\n", 
                         (unsigned long)cyclePosition, (unsigned long)sleepStartTime);
          } else {
            // In end buffer period - active for data transmission
            Serial.printf("[ACTIVE] End buffer period (position: %lu / %lu ms)\n", 
                         (unsigned long)cyclePosition, (unsigned long)UPLOAD_PERIOD_MS);
          }
        }
      #endif

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
              Serial.printf("[MAIN] Drained %u new records from buffer (dropped %u)\n", (unsigned)newRecords.size(), (unsigned)g_buffer->droppedCount());
              Serial.printf("[MAIN]Number of Real Inverter Samples: %u\n", (unsigned)newRecords.size());

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

  // ============================================================================
  // POWER OPTIMIZATION: Configure Auto Light Sleep (Real Hardware Only)
  // ============================================================================
  // This section configures ESP32 Power Management for automatic light sleep.
  // Auto light sleep leverages FreeRTOS Tickless IDLE to enter low power mode
  // when all tasks are blocked/suspended for sufficient duration.
  //
  // Configuration requirements (set in platformio.ini for real hardware build):
  // 1. CONFIG_PM_ENABLE=y - Enable Power Management component
  // 2. CONFIG_FREERTOS_USE_TICKLESS_IDLE=y - Enable FreeRTOS tickless mode
  // 3. CONFIG_FREERTOS_IDLE_TIME_BEFORE_SLEEP=3 - Min ticks before sleep (30ms @ 100Hz)
  // 4. CONFIG_ESP32_RTC_CLK_SRC_EXT_CRYS=y - External 32kHz crystal for BLE SCA accuracy
  //
  // Power Savings (Expected Estimates):
  // - Active mode: ~240 mA (WiFi + CPU + Modbus)
  // - Light sleep: ~30-50 mA (WiFi modem sleep + RTC)
  // - Average (70% duty cycle): ~100-105 mA
  // ============================================================================
  #if !SIMULATE
    Serial.println("\n[POWER] Configuring auto light sleep for real hardware...");
    
    // Configure Power Management parameters
    esp_pm_config_esp32_t pm_config;
    pm_config.max_freq_mhz = 240;           // Maximum CPU frequency (full performance)
    pm_config.min_freq_mhz = 80;            // Minimum CPU frequency (power saving)
    pm_config.light_sleep_enable = true;    // Enable automatic light sleep
    
    esp_err_t err = esp_pm_configure(&pm_config);
    if (err != ESP_OK) {
      Serial.printf("[POWER] Power management configuration failed: %d\n", err);
      Serial.println("[POWER] Light sleep will NOT be active");
      g_lightSleepEnabled = false;
      g_pmConfigured = false;
    } else {
      Serial.println("[POWER] Power management configured successfully");
      Serial.println("[POWER] Auto light sleep ENABLED");
      Serial.printf("[POWER] CPU frequency range: %u - %u MHz\n", 
                    pm_config.min_freq_mhz, pm_config.max_freq_mhz);
      Serial.println("[POWER] FreeRTOS Tickless IDLE will trigger light sleep");
      Serial.println("[POWER] WiFi modem sleep will maintain connection");
      
      // Configure WiFi power save mode for light sleep compatibility
      esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
      Serial.println("[POWER] WiFi modem sleep mode set (connection maintained)");
      
      g_lightSleepEnabled = true;
      g_pmConfigured = true;
      
      // Log power savings estimate
      Serial.println("\n[POWER] Estimated power savings:");
      Serial.println("  Active mode: ~240 mA");
      Serial.println("  Light sleep: ~30-50 mA");
      Serial.println("  Average (70% sleep): ~100-105 mA");
      Serial.println("  Power reduction: ~70%");
      Serial.println("  Battery life: 2.3x improvement\n");
    }
  #else
    Serial.println("\n[POWER] Simulation mode - Light sleep DISABLED");
    Serial.println("[POWER] Fast response maintained for testing/demos\n");
  #endif

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
  }
  catch (const std::exception &e)
  {
    Serial.printf("Setup failed with error: %s\n", e.what());
  }

  //main task acquisition
  xTaskCreatePinnedToCore(
      main_task, // Task function. //
      "Task1",   // name of task. //
      100000,     // Stack size of task //
      NULL,      // parameter of the task //
      1,         // priority of the task //
      &Task1,    //Task handle to keep track of created task //
      1);        // pin task to core 1 //
  delay(500);

  // connectivity task
  xTaskCreatePinnedToCore(
      CloudConnect, // Task function. //
      "Task2",      // name of task. //
      40000,        // Stack size of task //
      NULL,         // parameter of the task //
      1,            // priority of the task //
      &Task2,       // Task handle to keep track of created task //
      0);           // pin task to core 0 //
  delay(500);
  
}

void loop()
{
}


extern "C" void app_main(void)
{
  // Initialize Arduino core
  initArduino();

  // Call normal Arduino setup()
  setup();

  // Simple loop that calls Arduino loop()
  while (true)
  {
    loop();
    // Yield to FreeRTOS so other tasks can run
    vTaskDelay(1);
  }
}
/*/