#pragma once
#include <Arduino.h>


typedef struct {
    uint8_t poll_period_ms_addr = 0x00;
    uint8_t upload_period_ms_addr = 0x01;
    uint8_t buffer_capacity_addr = 0x02;
    uint8_t reg_req_id_1_addr = 0x03;//reserve fourth address
    //0b 0000_0000_0000_0000: id1
} ParamAddress;


extern bool config_changed;


static void updateConfig(uint8_t poll_period_ms, uint8_t upload_period_ms, uint8_t buffer_capacity, uint16_t reg_req_id_1);

uint8_t retrieveConfig(uint8_t addr);

static bool validateConfig(uint8_t poll_period_ms, uint8_t upload_period_ms, uint8_t buffer_capacity, uint16_t reg_req_id_1);

bool SaveConfig(byte* payload, unsigned int len);

void ApplyConfig();