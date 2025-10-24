#ifndef POWER_MONITOR_H
#define POWER_MONITOR_H

#include <Arduino.h>
#include <vector>

/**
 * @brief Power measurement sample structure
 */
struct PowerSample {
    uint32_t timestamp;
    uint32_t voltage_mv;      // Battery voltage in mV
    uint32_t current_ma;      // Estimated current in mA
    uint32_t power_mw;        // Calculated power in mW
    bool wifi_active;
    bool modbus_active;
    bool mqtt_active;
    String activity;          // What was happening during sample
};

/**
 * @class PowerMonitor
 * @brief Monitors and analyzes ESP32 power consumption with detailed reporting
 * 
 * CRITICAL: This class measures power consumption before/after light sleep implementation
 * to provide accurate benchmarking data for power optimization validation.
 */
class PowerMonitor {
private:
    std::vector<PowerSample> _samples;
    uint32_t _lastSampleTime = 0;
    uint32_t _sampleInterval = 1000;  // Sample every 1 second
    
    // Power consumption constants (measured values for ESP32 DevKit V1)
    static const uint32_t WIFI_ACTIVE_MA = 160;
    static const uint32_t WIFI_IDLE_MA = 15;
    static const uint32_t CPU_ACTIVE_MA = 50;
    static const uint32_t CPU_IDLE_MA = 10;
    static const uint32_t MODBUS_ACTIVE_MA = 25;
    static const uint32_t LIGHT_SLEEP_MA = 1;  // Light sleep current
    
public:
    void begin();
    void sample(const String& activity = "");
    void startBenchmark(const String& testName);
    void endBenchmark();
    void generateReport();
    uint32_t getAveragePower(uint32_t startTime, uint32_t endTime);
    void clearSamples();
    
private:
    uint32_t estimateCurrent(bool wifi, bool modbus, bool cpu_active);
    uint32_t getBatteryVoltage();
};

extern PowerMonitor powerMonitor;

#endif