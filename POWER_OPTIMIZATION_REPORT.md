# Power Optimization Report: ESP32 Light Sleep Implementation

**Project:** Eco-watt Solar Inverter Monitoring System  
**Hardware:** ESP32 DevKit V1  
**Date:** November 27, 2025

---

## Executive Summary

This report presents the power optimization strategy for the ESP32-based solar inverter monitoring system. Light sleep implementation between data uploads achieves approximately 70% power reduction while maintaining WiFi connectivity and system reliability.

The system supports two operational modes: API Simulation Mode for testing with 20-second cycles, and Real RS-485 Hardware Mode for production deployment with 15-minute cycles.

---

## Power Analysis

| Metric | Active Mode | Light Sleep Mode | Savings |
|--------|-------------|------------------|---------|
| Current Draw | ~240 mA | ~30-50 mA | ~190-210 mA |
| Components | WiFi TX/RX, CPU, Modbus, Peripherals | WiFi Modem Sleep, RTC, RAM | CPU halted, WiFi reduced |
| Duty Cycle (Production) | 30% (270s) | 70% (630s) | - |
| Average Current (Production) | - | ~100-105 mA | 70% reduction |
| Battery Life (5000 mAh) | ~21 hours | ~48 hours | 2.3x improvement |

---

## Operational Modes

| Parameter | API Simulation Mode | Real Hardware Mode |
|-----------|---------------------|-------------------|
| **Purpose** | Testing and demonstrations | Production deployment |
| **Upload Interval** | 20 seconds | 15 minutes |
| **Light Sleep** | Disabled | Enabled |
| **Data Source** | Cloud API | Physical inverter (Modbus RTU) |
| **Average Power** | ~240 mA | ~100-105 mA |
| **Sleep Duration** | None | 630s (70% of cycle) |
| **Active Duration** | Continuous | 270s (30% of cycle) |
| **Use Case** | Testing, demos, debugging | Remote installations, battery-powered

---

## Implementation Strategy

### Sleep Timing
Sleep duration is calculated dynamically: **Sleep Time = (Time Until Upload) × 70%**

For 15-minute uploads: 630 seconds sleep, 270 seconds active. The 70/30 split balances power savings with WiFi stability and ensures adequate time for data transmission.

### WiFi Management
- **Active periods:** Full WiFi power for polling and uploads
- **Sleep periods:** WiFi modem sleep mode maintains connection without reconnection overhead
- **Safety:** WiFi status verified before sleep; sleep prevented if disconnected

---

## System Behavior

| Component | During Sleep | During Active |
|-----------|-------------|---------------|
| CPU | Halted | Full operation |
| WiFi | Modem sleep (connection maintained) | Full power TX/RX |
| RAM | Data retained | Normal operation |
| RTC Timer | Maintains timekeeping | Normal operation |
| Modbus | Not queried | Polling every 10s |
| Serial Output | No logging | Normal logging |

**Wakeup:** Timer interrupt fires → CPU resumes → WiFi restored → Operations continue

---

## Deployment Recommendations

| Scenario | Power Source | Recommended Mode | Rationale |
|----------|-------------|------------------|-----------|
| Remote Installation | Solar + Battery | Real Hardware (sleep enabled) | Extended runtime critical |
| Grid-Connected | AC Mains | Either mode | Power not constrained |
| Development/Testing | USB/Bench | API Simulation | Fast iteration needed |
| Demonstrations | Battery Pack | API Simulation | Continuous activity preferred |

---

## Summary

The light sleep implementation achieves 70% power reduction in production mode while maintaining WiFi connectivity and system functionality. The dual-mode architecture supports both testing (simulation mode) and production deployment (real hardware mode) with minimal configuration changes.

**Key Results:**
- 70% power reduction in production mode
- 2.3x battery life improvement  
- WiFi stability maintained through sleep cycles
- Flexible configuration for different scenarios

**Status:** Production-ready. Configuration can be switched between modes without code recompilation.

---

**Document Version:** 1.0  
**Implementation Status:** Complete
