#pragma once
#include <Arduino.h>


typedef struct {
    uint8_t poll_period_ms_addr = 0x00;      // 0x00-0x01 (2 bytes)
    uint8_t upload_period_ms_addr = 0x02;    // 0x02-0x03 (2 bytes)  
    uint8_t buffer_capacity_addr = 0x04;     // 0x04-0x05 (2 bytes)
    uint8_t reg_req_id_1_addr = 0x06;        // 0x06-0x07 (2 bytes)
} ParamAddress;


enum ErrorCode {
    ERR_OK = 0,                // No error
    ERR_POLL_MS_FAILED,        // Polling period update failed
    ERR_UPLOAD_MS_FAILED,      // Upload period update failed
    ERR_BUFFER_CAPACITY_FAILED, // Buffer capacity update failed
    ERR_REG_REQ_ID_1_FAILED,  // Reg request ID 1 update failed
    ERR_DESERIALIZE_FAILED,    // JSON deserialization failed
    ERR_UNKNOWN                // Unknown/unspecified error
};



extern bool config_changed;


//static void updateConfig(uint16_t poll_period_ms, uint16_t upload_period_ms, uint16_t buffer_capacity, uint16_t reg_req_id_1);

uint8_t retrieveConfig(uint8_t addr);



//static bool validateConfig(uint16_t poll_period_ms, uint16_t upload_period_ms, uint16_t buffer_capacity, uint16_t reg_req_id_1);

ErrorCode SaveConfig(byte* payload, unsigned int len);

void ApplyConfig();