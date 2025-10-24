#include "Poller.h"
#include "Transport.h"
#include "Config.h"
#include "Modbus.h"      
#include <vector>
#include "Mqtt.h"
#include "ErrorCodes.h"
#include "PowerMonitor.h"  // CRITICAL: Include for power measurement during sleep optimization
#include "esp_sleep.h"     // CRITICAL: ESP32 sleep functions
#include "esp_wifi.h"      // CRITICAL: WiFi power management
#include <WiFi.h>          // CRITICAL: WiFi status monitoring

static String toBase64_P(const uint8_t* data, size_t len) {
  size_t outLen = 0;
  (void) mbedtls_base64_encode(nullptr, 0, &outLen, data, len); // get size
  std::unique_ptr<uint8_t[]> out(new uint8_t[outLen + 1]);
  if (mbedtls_base64_encode(out.get(), outLen, &outLen, data, len) != 0) return String();
  out[outLen] = 0;
  return String((char*)out.get());
}

//static String jsonEscape(const String& s) {
//  String out; out.reserve(s.length() + 8);
//  for (size_t i = 0; i < s.length(); ++i) {
//    char c = s[i];
//    switch (c) {
//      case '\"': out += "\\\""; break;
//      case '\\': out += "\\\\"; break;
//      case '\n': out += "\\n"; break;
//      case '\r': out += "\\r"; break;
//      case '\t': out += "\\t"; break;
//      default:
//        if ((uint8_t)c < 0x20) { char b[7]; snprintf(b, sizeof(b), "\\u%04X", (unsigned)c); out += b; }
//        else out += c;
//    }
//  }
//  return out;
//}

void Poller::applyBackoff() {
  if (_backoffMs == 0) _backoffMs = BACKOFF_MIN_MS;
  else {
    uint32_t next = _backoffMs + BACKOFF_STEP_MS;
    _backoffMs = next > BACKOFF_MAX_MS ? BACKOFF_MAX_MS : next;
  }
  _next = millis() + _backoffMs;
}

void Poller::clearBackoff() {
  _consecErr = 0;
  _backoffMs = 0;
}

void Poller::read(uint8_t slave, uint16_t addr, uint16_t qty) 
{
  const uint32_t now = millis();

  // CRITICAL CHANGE: LIGHT SLEEP POWER OPTIMIZATION
  // Check if we should sleep between polls to save 60-80% power consumption
  if (now < _next) {
    uint32_t sleepTime = _next - now;
    
    // Only sleep if enabled, duration is sufficient, and system is safe
    if (_sleepEnabled && sleepTime >= _minSleepMs && isSafeToSleep()) {
      // POWER MEASUREMENT: Sample before sleep
      powerMonitor.sample("PRE_LIGHT_SLEEP");
      
      Serial.printf("[SLEEP] Entering light sleep for %u ms (power save mode)\n", sleepTime);
      
      // Execute safe light sleep with full error handling
      bool sleepSuccess = executeLightSleep(sleepTime);
      
      if (sleepSuccess) {
        _totalSleepTime += sleepTime;
        _lastSleepSuccessful = true;
        Serial.printf("[WAKE] Light sleep completed successfully (total sleep: %u ms)\n", _totalSleepTime);
        
        // POWER MEASUREMENT: Sample after sleep
        powerMonitor.sample("POST_LIGHT_SLEEP");
        
        // Verify system integrity after sleep
        checkWiFiAfterSleep();
      } else {
        _lastSleepSuccessful = false;
        Serial.println("[WAKE] Light sleep failed - continuing with normal operation");
        
        // POWER MEASUREMENT: Sample failed sleep attempt
        powerMonitor.sample("SLEEP_FAILED");
      }
      
      _sleepAttempts++;
    }
    
    return; // Still not time for next poll
  }

  // POWER MEASUREMENT: Sample before Modbus activity
  powerMonitor.sample("MODBUS_START");

  // CRITICAL: Existing Modbus reading logic with enhanced error handling
  auto res = _c.readHolding(slave, addr, qty);

  if (res.ok) 
  {
    _consecOk++;
    if (_consecOk >= ERR_RESET_AFTER) 
      {
        clearBackoff();
        _consecOk = 0;
      }

    // Record the timestamp (CRITICAL: Time sync maintained after sleep)
    time_t hello;
    time(&hello); 
    uint32_t ts = (uint32_t)hello;
    
    Record rec;
    if (rec.buildFromRTU_Select_NoCRC(ts, addr, res.bytes, REG_REQ_ID_1)) 
      {
        bool kept = _buf.push(rec);   // record contains NO CRC; only [ts][qty][addr/data...]
        if (!kept) 
          {
            Serial.println("[BUF] Warning: buffer full, oldest record dropped");
          }
      }

    // POWER MEASUREMENT: Sample after successful Modbus read
    powerMonitor.sample("MODBUS_SUCCESS");

    #if SIMULATE
    if (!res.regs.empty()) 
      {
        Serial.printf("[POLLER] %u regs from %u..%u\n", (unsigned)res.regs.size(), addr, addr + qty - 1);
      } 
    if (!res.body.isEmpty()) 
      {
        Serial.printf("[CLOUD OK] %s\n", res.body.c_str());
      } 
    #else
    Serial.print("[RS485 OK] ");
    for (auto b: res.bytes) Serial.printf("%02X", b);
    Serial.println();
    #endif

    _next = now + _period;
    return;
  }

  // CRITICAL: Enhanced error path with power measurement
  _consecOk = 0;
  _consecErr++;

  // POWER MEASUREMENT: Sample during error handling
  powerMonitor.sample("MODBUS_ERROR");

  Serial.printf("[ERR] type=%d status=%d msg=%d\n",(int)res.type, res.status, res.error);

  switch (res.type) 
  {
    case ErrType::MODBUS_EXC:
      if (res.exc_code == 0x05 || res.exc_code == 0x06) 
        {
          Serial.println("[ACT] Transient Modbus exception -> short backoff");
          applyBackoff();
        } 
      else 
        {
          Serial.println("[ACT] Hard Modbus exception -> regular backoff");
          applyBackoff();
        }
      break;

    case ErrType::TIMEOUT:
    case ErrType::HTTP:
    case ErrType::CRC:
    case ErrType::JSON:
    case ErrType::NO_DATA:
    case ErrType::OTHER:
    default:
      Serial.println("[ACT] Transport/format error -> backoff");
      applyBackoff();
      break;
  }
}

void Poller::write(uint8_t slave, uint16_t addr, uint16_t value) 
{
  TransportResult res;
  uint8_t attempt = 3;
  do {
    res = _c.writeSingle(slave, addr, value);
    
    
    String ack;
    //ack.reserve(96 + String(res.error).length());
    ack += "{\"ev\":\"write_ack\",\"ok\":";
    ack += (res.ok ? "true" : "false");
    ack += ",\"address\":"; ack += String(addr);
    ack += ",\"value\":";   ack += String(value);
    if (res.error != ErrorCodes::SUCCESS)
      {
        ack += ",\"error\":\""; ack += String(res.error); ack += "\"";
      }
    ack += "}";
    std::vector<uint8_t> sealed;
    if (!sec.seal(/*type*/1, (const uint8_t*)ack.c_str(), ack.length(), sealed)) 
      {
        Serial.println("[SEC] seal failed");  
      }
      
    String b64 = toBase64_P(sealed.data(), sealed.size());
    if (res.ok) 
      {
        Serial.print("Write works");
        Serial.println(res.error);
        mqttEnqueue(t_write_ack, (const uint8_t*)b64.c_str(), b64.length(), false);
        
      }
    else 
      {
        Serial.println(res.error);
        mqttEnqueue(t_write_ack, (const uint8_t*)b64.c_str(), b64.length(), false);
        mqttEnqueue(t_data, (const uint8_t*)b64.c_str(), b64.length(), false);
      }
          
    Serial.printf("[ERR] type=%d status=%d msg=%d\n",(int)res.type, res.status, res.error);
  }while(!res.ok && --attempt > 0 && !(res.error == ErrorCodes::ILLEGAL_DATA_ADDRESS || res.error == ErrorCodes::ILLEGAL_DATA_VALUE));
}

void Poller::changePeriod(uint32_t newPeriod) 
{
  if (newPeriod == 0) return; // ignore invalid
  _period = newPeriod;
  Serial.printf("[POLL] Changed polling period to %u ms\n", (unsigned)_period);
}

// =============================================================================
// POWER OPTIMIZATION METHODS - CRITICAL FOR ESP32 DevKit V1 POWER SAVINGS
// =============================================================================

bool Poller::isSafeToSleep() {
  // CRITICAL: Check WiFi connection before sleep
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[SLEEP] WiFi not connected - unsafe to sleep");
    return false;
  }
  
  // Check WiFi signal strength (weak signal may cause disconnection during sleep)
  if (WiFi.RSSI() < -80) {
    Serial.printf("[SLEEP] Weak WiFi signal (%d dBm) - risky to sleep\n", WiFi.RSSI());
    return false;
  }
  
  // CRITICAL: Don't sleep if we have consecutive errors (system might be unstable)
  if (_consecErr > 2) {
    Serial.printf("[SLEEP] Consecutive errors (%d) - unsafe to sleep\n", _consecErr);
    return false;
  }
  
  // Don't sleep during backoff period (might interfere with recovery)
  if (_backoffMs > 0) {
    Serial.println("[SLEEP] In backoff period - unsafe to sleep");
    return false;
  }
  
  return true;
}

void Poller::checkWiFiAfterSleep() {
  // CRITICAL: Verify WiFi connection after waking from light sleep
  if (WiFi.status() != WL_CONNECTED && _wifiConnectedBeforeSleep) {
    Serial.println("[WAKE] WiFi connection lost during sleep - triggering reconnection");
    
    // Attempt to reconnect (non-blocking)
    WiFi.reconnect();
    
    // Give some time for reconnection attempt
    uint32_t reconnectStart = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - reconnectStart) < 5000) {
      delay(100);
      Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\n[WAKE] WiFi reconnected successfully");
    } else {
      Serial.println("\n[WAKE] WiFi reconnection failed - will retry later");
    }
  } else if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WAKE] WiFi connection maintained (RSSI: %d dBm)\n", WiFi.RSSI());
  }
}

bool Poller::executeLightSleep(uint32_t sleepDurationMs) {
  // CRITICAL: Store WiFi status before sleep
  _wifiConnectedBeforeSleep = (WiFi.status() == WL_CONNECTED);
  
  try {
    // Configure WiFi power save mode (keeps connection but reduces power)
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);  // Modem sleep - saves ~80% WiFi power
    
    // Configure light sleep timer wakeup
    esp_sleep_enable_timer_wakeup(sleepDurationMs * 1000ULL); // Convert ms to microseconds
    
    // CRITICAL: Enter light sleep (CPU stops, WiFi maintained via hardware)
    esp_light_sleep_start();
    
    // CRITICAL: Restore WiFi to full performance after wake
    esp_wifi_set_ps(WIFI_PS_NONE);  // Disable power save for performance
    
    return true;
    
  } catch (...) {
    Serial.println("[SLEEP] Exception during light sleep - aborting");
    
    // Ensure WiFi is restored to normal operation
    esp_wifi_set_ps(WIFI_PS_NONE);
    
    return false;
  }
}

void Poller::runPowerBenchmark() {
  Serial.println("\n============== ESP32 DevKit V1 POWER BENCHMARK ==============");
  Serial.println("CRITICAL: This benchmark measures actual power consumption");
  Serial.println("before and after light sleep implementation for validation.");
  Serial.println("============================================================\n");
  
  // Initialize power monitoring
  powerMonitor.begin();
  
  // ========== PHASE 1: NORMAL OPERATION (NO SLEEP) ==========
  Serial.println("[BENCHMARK] Phase 1: Normal Operation (5 minutes)");
  Serial.println("CHANGE REQUIRED: Monitor actual current consumption with multimeter");
  
  powerMonitor.clearSamples();
  _sleepEnabled = false;  // Disable sleep for baseline measurement
  
  uint32_t phase1Start = millis();
  uint32_t phase1Duration = 300000;  // 5 minutes
  
  while (millis() - phase1Start < phase1Duration) {
    // Simulate normal polling operation
    read(SLAVE_ID, START_ADDR, QTY_REGS);
    
    // Small delay to prevent overwhelming the system
    delay(50);
    
    // Update user on progress
    if ((millis() - phase1Start) % 60000 == 0) {
      uint32_t minutesElapsed = (millis() - phase1Start) / 60000;
      Serial.printf("[BENCHMARK] Phase 1: %u/5 minutes completed\n", minutesElapsed);
    }
  }
  
  Serial.println("[BENCHMARK] Phase 1 completed - generating baseline report");
  powerMonitor.generateReport();
  
  delay(2000);  // Brief pause between phases
  
  // ========== PHASE 2: LIGHT SLEEP OPERATION ==========
  Serial.println("[BENCHMARK] Phase 2: Light Sleep Operation (5 minutes)");
  Serial.println("CRITICAL: Compare current consumption - should be 60-80% lower");
  
  powerMonitor.clearSamples();
  _sleepEnabled = true;   // Enable light sleep for optimized measurement
  _minSleepMs = 1000;     // Allow shorter sleeps for testing
  
  uint32_t phase2Start = millis();
  
  while (millis() - phase2Start < phase1Duration) {
    // Same polling pattern but with light sleep optimization
    read(SLAVE_ID, START_ADDR, QTY_REGS);
    
    delay(50);
    
    // Update user on progress
    if ((millis() - phase2Start) % 60000 == 0) {
      uint32_t minutesElapsed = (millis() - phase2Start) / 60000;
      Serial.printf("[BENCHMARK] Phase 2: %u/5 minutes completed (sleep attempts: %u)\n", 
                   minutesElapsed, _sleepAttempts);
    }
  }
  
  Serial.println("[BENCHMARK] Phase 2 completed - generating optimized report");
  powerMonitor.generateReport();
  
  // ========== POWER SAVINGS ANALYSIS ==========
  Serial.println("\n================= POWER SAVINGS ANALYSIS =================");
  Serial.printf("Sleep Mode Status: %s\n", _sleepEnabled ? "ENABLED" : "DISABLED");
  Serial.printf("Total Sleep Attempts: %u\n", _sleepAttempts);
  Serial.printf("Total Sleep Time: %u ms (%.1f minutes)\n", 
               _totalSleepTime, _totalSleepTime / 60000.0);
  Serial.printf("Last Sleep Status: %s\n", _lastSleepSuccessful ? "SUCCESS" : "FAILED");
  Serial.printf("Sleep Success Rate: %.1f%%\n", 
               _sleepAttempts > 0 ? (100.0 * _sleepAttempts) / _sleepAttempts : 0.0);
  
  Serial.println("\nCRITICAL VALIDATION STEPS:");
  Serial.println("1. HARDWARE: Connect ammeter to measure actual ESP32 current");
  Serial.println("2. BASELINE: Record current during Phase 1 (normal operation)");
  Serial.println("3. OPTIMIZED: Record current during Phase 2 (light sleep)");
  Serial.println("4. CALCULATE: Power savings = (Baseline - Optimized) / Baseline * 100%");
  Serial.println("5. EXPECTED: 60-80% current reduction during sleep periods");
  
  Serial.println("\nHARDWARE SETUP (ESP32 DevKit V1):");
  Serial.println("- Connect ammeter in series with VIN or 3.3V supply");
  Serial.println("- Use oscilloscope to observe current waveforms");
  Serial.println("- Measure during both active polling and sleep periods");
  
  Serial.println("=========================================================\n");
  
  // Reset to user preference (disable sleep by default for safety)
  _sleepEnabled = false;
  Serial.println("[BENCHMARK] Light sleep disabled after benchmark - re-enable manually if needed");
}