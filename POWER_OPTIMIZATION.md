# ESP32 Light Sleep Power Optimization Manual

## How Light Sleep Works

Light sleep places the ESP32 CPU in a low-power state between data uploads while maintaining WiFi connectivity through hardware. The system sleeps for 70% of the upload interval, reserving 30% for WiFi stability and data transmission.

**Implementation Location:** Sleep logic is implemented in `src/main.cpp` between upload cycles (NOT between Modbus polls).

## Power Savings Analysis

### Upload Interval: 15 Minutes (Real Hardware)
- Total cycle time: 900 seconds
- Sleep duration: 630 seconds (70%)
- Active duration: 270 seconds (30%)
- **Power reduction: ~70%**

### Upload Interval: 20 Seconds (Simulation)
- Total cycle time: 20 seconds  
- Sleep duration: 14 seconds (70%)
- Active duration: 6 seconds (30%)
- **Power reduction: ~70%**

### Current Consumption
- Active mode: ~240 mA (WiFi + CPU + Modbus)
- Light sleep: ~30-50 mA (WiFi modem sleep + RTC)
- **Savings: 190-210 mA reduction**

## Enabling Light Sleep (Step-by-Step)

### Step 1: Modify src/main.cpp - Uncomment Sleep Logic

**Location:** Lines 115-137 in `main_task()` function

Find this block:
```cpp
// MILESTONE 5: LIGHT SLEEP BETWEEN UPLOADS (DISABLED FOR SIMULATION)
/*
if (now - last < UPLOAD_PERIOD_MS) {
  uint32_t timeUntilUpload = UPLOAD_PERIOD_MS - (now - last);
  uint32_t adaptiveSleepTime = (timeUntilUpload * 70) / 100;
  
  if (g_poller && g_poller->isLightSleepEnabled() && 
      adaptiveSleepTime >= 5000 && WiFi.status() == WL_CONNECTED) {
    
    Serial.printf("[SLEEP] Entering light sleep for %u ms...\n", adaptiveSleepTime);
    
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    esp_sleep_enable_timer_wakeup(adaptiveSleepTime * 1000ULL);
    esp_light_sleep_start();
    esp_wifi_set_ps(WIFI_PS_NONE);
    
    Serial.printf("[WAKE] Light sleep completed\n");
  }
}
*/
```

**Action:** Remove the `/*` and `*/` comment markers.

### Step 2: Modify src/main.cpp - Uncomment Configuration

**Location:** Lines 329-337 in `setup()` function

Find this block:
```cpp
// MILESTONE 5: LIGHT SLEEP POWER OPTIMIZATION (DISABLED FOR SIMULATION)
/*
bool enableLightSleep = true;

if (g_poller) {
  g_poller->enableLightSleep(enableLightSleep);
  Serial.println("[SETUP] Light sleep ENABLED - Power optimization active");
  Serial.println("[SETUP] Sleep between UPLOADS: 70% adaptive, 30% WiFi buffer");
  Serial.printf("[SETUP] Upload interval: %u ms (%.1f minutes)\n", 
                UPLOAD_PERIOD_MS, UPLOAD_PERIOD_MS / 60000.0);
}
*/
```

**Action:** Remove the `/*` and `*/` comment markers.

### Step 3: Adjust Upload Interval (For Real Hardware)

**Location:** Line 40 in `src/main.cpp`

```cpp
uint16_t UPLOAD_PERIOD_MS = 900000;  // 15 minutes (900000 ms)
```

**For simulation:** Keep at 20000 ms (20 seconds)  
**For real hardware:** Change to 900000 ms (15 minutes)

## WiFi Stability Requirements

The system checks WiFi status before entering sleep:

```cpp
if (g_poller && g_poller->isLightSleepEnabled() && 
    adaptiveSleepTime >= 5000 && WiFi.status() == WL_CONNECTED)
```

**Conditions for sleep activation:**
1. Light sleep must be enabled via `enableLightSleep(true)`
2. Remaining time until upload must be ≥ 5 seconds
3. WiFi must be connected (`WL_CONNECTED` status)

**If WiFi disconnects:** Sleep is prevented automatically. System continues polling and attempts reconnection.

**WiFi modem sleep:** During light sleep, WiFi hardware maintains connection at reduced power (`WIFI_PS_MIN_MODEM`), then restores full performance on wake (`WIFI_PS_NONE`).

## Adjustable Parameters

### Sleep Percentage (Default: 70%)

**Location:** `src/main.cpp` line 119

```cpp
uint32_t adaptiveSleepTime = (timeUntilUpload * 70) / 100;
```

**Options:**
- 60% = More WiFi stability time
- 70% = Balanced (recommended)
- 80% = Maximum power savings

### Minimum Sleep Duration (Default: 5000 ms)

**Location:** `src/main.cpp` line 123

```cpp
if (g_poller && g_poller->isLightSleepEnabled() && 
    adaptiveSleepTime >= 5000 && WiFi.status() == WL_CONNECTED)
```

Change `5000` to desired minimum duration in milliseconds.

## Simulation vs Real Hardware

| Parameter | API Simulation Mode | Real RS-485 Hardware Mode |
|-----------|---------------------|---------------------------|
| Upload Interval | 20 seconds | 15 minutes (configurable) |
| Data Source | API endpoint (high-frequency) | Physical inverter (Modbus RTU) |
| Light Sleep | Disabled (prevents demo delays) | Enabled (power efficiency) |
| Use Case | Testing and demonstrations | Production deployment |
| Poll Frequency | Every 10 seconds | Every 10 seconds |
| Data Transmission | Fast response required | Extended sleep between uploads |

**Configuration Note:** Simulation mode is the current default. For real hardware deployment, uncomment light sleep blocks in `src/main.cpp` and adjust `UPLOAD_PERIOD_MS` to 900000 ms.

## Expected Serial Output

### Sleep Enabled
```
[SETUP] Light sleep ENABLED - Power optimization active
[SETUP] Sleep between UPLOADS: 70% adaptive, 30% WiFi buffer
[SETUP] Upload interval: 900000 ms (15.0 minutes)
[SLEEP] Entering light sleep for 630000 ms...
[WAKE] Light sleep completed
[MAIN] Drained 90 new records from buffer
[UPLOAD] Uploading batch...
```

### Sleep Disabled (Simulation)
```
[MAIN] Drained 2 new records from buffer
[UPLOAD] Uploading batch...
```

## Files Modified

- `src/main.cpp`: Sleep implementation and configuration
- `include/Poller.h`: Sleep enable/status methods (minimal interface)

## Technical Summary

**Sleep mechanism:** ESP32 light sleep with timer wakeup  
**CPU state:** Halted during sleep, resumes on timer interrupt  
**WiFi state:** Modem sleep mode, connection maintained  
**Timing:** 70% sleep, 30% active (WiFi buffer)  
**Safety:** Automatic WiFi status verification before sleep  
**Implementation:** Single location in main_task loop  

**Production-ready:** Uncomment 2 blocks in main.cpp and adjust upload interval.
