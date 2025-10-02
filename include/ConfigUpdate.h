#pragma once
#include <Arduino.h>


typedef struct {
    uint8_t poll_period_ms_addr = 0x00;      // 0x00-0x01 (2 bytes)
    uint8_t upload_period_ms_addr = 0x02;    // 0x02-0x03 (2 bytes)  
    uint8_t buffer_capacity_addr = 0x04;     // 0x04-0x05 (2 bytes)
    uint8_t reg_req_id_1_addr = 0x06;        // 0x06-0x07 (2 bytes)
} ParamAddress;


extern bool config_changed;


static void updateConfig(uint16_t poll_period_ms, uint16_t upload_period_ms, uint16_t buffer_capacity, uint16_t reg_req_id_1);

uint8_t retrieveConfig(uint8_t addr);

void InitializeConfig();

static bool validateConfig(uint16_t poll_period_ms, uint16_t upload_period_ms, uint16_t buffer_capacity, uint16_t reg_req_id_1);

bool SaveConfig(byte* payload, unsigned int len);

void ApplyConfig();