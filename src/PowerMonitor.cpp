#include "PowerMonitor.h"
#include "WiFi.h"
#include "esp_wifi.h"
#include "esp_sleep.h"
#include "driver/adc.h"

PowerMonitor powerMonitor;

void PowerMonitor::begin() {
    // Configure ADC for battery voltage measurement (optional - uses nominal 3.3V if not connected)
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11); // GPIO36
    
    Serial.println("[POWER] Power monitor initialized for ESP32 DevKit V1");
    _samples.reserve(3600); // Reserve space for 1 hour of samples
}

void PowerMonitor::sample(const String& activity) {
    uint32_t now = millis();
    
    if (now - _lastSampleTime < _sampleInterval) {
        return; // Not time for next sample yet
    }
    
    PowerSample sample;
    sample.timestamp = now;
    sample.voltage_mv = getBatteryVoltage();
    sample.wifi_active = (WiFi.status() == WL_CONNECTED);
    sample.modbus_active = activity.indexOf("MODBUS") != -1;
    sample.mqtt_active = activity.indexOf("MQTT") != -1;
    sample.activity = activity;
    
    // Estimate current consumption based on active components
    bool cpu_active = activity.indexOf("SLEEP") == -1; // CPU idle during sleep
    sample.current_ma = estimateCurrent(sample.wifi_active, sample.modbus_active, cpu_active);
    sample.power_mw = (sample.voltage_mv * sample.current_ma) / 1000;
    
    _samples.push_back(sample);
    _lastSampleTime = now;
    
    // Limit samples to prevent memory overflow on ESP32
    if (_samples.size() > 3600) {
        _samples.erase(_samples.begin(), _samples.begin() + 600); // Remove oldest 10 minutes
    }
}

uint32_t PowerMonitor::estimateCurrent(bool wifi, bool modbus, bool cpu_active) {
    uint32_t current = 0;
    
    // Base ESP32 current (DevKit V1 specific)
    current += cpu_active ? CPU_ACTIVE_MA : CPU_IDLE_MA;
    
    // WiFi current consumption
    if (wifi) {
        wifi_ps_type_t ps_type;
        esp_wifi_get_ps(&ps_type);
        
        if (ps_type == WIFI_PS_NONE) {
            current += WIFI_ACTIVE_MA; // Full power WiFi
        } else {
            current += WIFI_IDLE_MA;   // Modem sleep WiFi
        }
    }
    
    // Modbus/RS485 current
    if (modbus) {
        current += MODBUS_ACTIVE_MA;
    }
    
    return current;
}

uint32_t PowerMonitor::getBatteryVoltage() {
    // Read ADC value from GPIO36 (assuming external voltage measurement)
    int adc_value = adc1_get_raw(ADC1_CHANNEL_0);
    
    // Convert to voltage (assuming 3.3V reference and 2:1 voltage divider)
    uint32_t voltage_mv = (adc_value * 3300 * 2) / 4095;
    
    // If no external voltage measurement connected, use nominal 3.3V
    if (voltage_mv < 2000 || voltage_mv > 5000) {
        voltage_mv = 3300; // Default to 3.3V for DevKit V1
    }
    
    return voltage_mv;
}

void PowerMonitor::clearSamples() {
    _samples.clear();
    Serial.println("[POWER] Sample history cleared");
}

void PowerMonitor::generateReport() {
    if (_samples.empty()) {
        Serial.println("[POWER] No samples collected for report");
        return;
    }
    
    Serial.println("\n==================== POWER CONSUMPTION REPORT ====================");
    
    // Calculate comprehensive statistics
    uint32_t totalPower = 0;
    uint32_t maxPower = 0;
    uint32_t minPower = UINT32_MAX;
    uint32_t wifiOnTime = 0;
    uint32_t modbusOnTime = 0;
    uint32_t sleepTime = 0;
    
    for (const auto& sample : _samples) {
        totalPower += sample.power_mw;
        if (sample.power_mw > maxPower) maxPower = sample.power_mw;
        if (sample.power_mw < minPower) minPower = sample.power_mw;
        if (sample.wifi_active) wifiOnTime++;
        if (sample.modbus_active) modbusOnTime++;
        if (sample.activity.indexOf("SLEEP") != -1) sleepTime++;
    }
    
    uint32_t avgPower = totalPower / _samples.size();
    uint32_t testDurationMin = (_samples.back().timestamp - _samples.front().timestamp) / 60000;
    
    // Print detailed results
    Serial.printf("Test Duration: %u minutes (%u seconds)\n", testDurationMin, testDurationMin * 60);
    Serial.printf("Total Samples: %u samples\n", _samples.size());
    Serial.printf("Average Power: %u mW\n", avgPower);
    Serial.printf("Peak Power: %u mW\n", maxPower);
    Serial.printf("Minimum Power: %u mW\n", minPower);
    Serial.printf("Power Range: %u mW\n", maxPower - minPower);
    
    // Activity analysis
    Serial.printf("WiFi Active: %u%% of time\n", (wifiOnTime * 100) / _samples.size());
    Serial.printf("Modbus Active: %u%% of time\n", (modbusOnTime * 100) / _samples.size());
    Serial.printf("Sleep Mode: %u%% of time\n", (sleepTime * 100) / _samples.size());
    
    // Energy calculations
    uint32_t energyMwh = (avgPower * testDurationMin) / 60; // Convert mW*min to mWh
    Serial.printf("Energy Consumed: %u mWh\n", energyMwh);
    
    // Projections for battery life calculations
    uint32_t dailyWh = (avgPower * 24) / 1000;      // Daily energy in Wh
    uint32_t monthlyWh = dailyWh * 30;               // Monthly energy in Wh
    uint32_t yearlyKwh = (monthlyWh * 12) / 1000;    // Yearly energy in kWh
    
    Serial.printf("Projected Daily: %u Wh\n", dailyWh);
    Serial.printf("Projected Monthly: %u Wh\n", monthlyWh);
    Serial.printf("Projected Yearly: %u kWh\n", yearlyKwh);
    
    // Battery life estimation (common battery capacities)
    Serial.println("\n--- Battery Life Estimates ---");
    uint32_t batteryCapacities[] = {1000, 2000, 5000, 10000}; // mAh
    const char* batteryNames[] = {"1000mAh", "2000mAh", "5000mAh", "10000mAh"};
    
    for (int i = 0; i < 4; i++) {
        // Battery life in hours = (capacity_mAh * voltage_V) / power_mW
        uint32_t batteryLifeHours = (batteryCapacities[i] * 3300) / avgPower; // Using 3.3V
        uint32_t batteryLifeDays = batteryLifeHours / 24;
        
        Serial.printf("%s Battery Life: %u hours (%u days)\n", 
                     batteryNames[i], batteryLifeHours, batteryLifeDays);
    }
    
    Serial.println("===================================================================\n");
}

uint32_t PowerMonitor::getAveragePower(uint32_t startTime, uint32_t endTime) {
    if (_samples.empty()) return 0;
    
    uint32_t totalPower = 0;
    uint32_t count = 0;
    
    for (const auto& sample : _samples) {
        if (sample.timestamp >= startTime && sample.timestamp <= endTime) {
            totalPower += sample.power_mw;
            count++;
        }
    }
    
    return count > 0 ? totalPower / count : 0;
}