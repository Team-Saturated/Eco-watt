
#include "ConfigUpdate.h"
#include <EEPROM.h>
#include <ArduinoJson.h>
#include "Config.h"

static void updateConfig(uint8_t poll_period_ms, uint8_t upload_period_ms, uint8_t buffer_capacity, uint16_t reg_req_id_1) {
    ParamAddress paramAddress;

    // Update poll period
    EEPROM.write(paramAddress.poll_period_ms_addr, poll_period_ms);

    // Update upload period
    EEPROM.write(paramAddress.upload_period_ms_addr, upload_period_ms);

    // Update buffer capacity
    EEPROM.write(paramAddress.buffer_capacity_addr, buffer_capacity);

    // Update register request ID 1 (2 bytes)
    EEPROM.write(paramAddress.reg_req_id_1_addr, (reg_req_id_1 >> 8) & 0xFF); // High byte
    EEPROM.write(paramAddress.reg_req_id_1_addr + 1, reg_req_id_1 & 0xFF);     // Low byte

    

    // Commit changes to EEPROM
    EEPROM.commit();
}

uint8_t retrieveConfig(uint8_t addr) {
    return EEPROM.read(addr);
}


static bool validateConfig(uint8_t poll_period_ms, uint8_t upload_period_ms, uint8_t buffer_capacity, uint16_t reg_req_id_1) {
    // Example validation rules
    

    return true;
}   

bool SaveConfig(byte* payload, unsigned int len) 
{
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload, len);
    if (error) 
    {
        return false;
    }
    if (doc.containsKey("poll_period_ms") && doc.containsKey("upload_period_ms") &&
        doc.containsKey("buffer_capacity") && doc.containsKey("reg_req_id_1")) 
    {
        uint8_t new_poll_period_ms = doc["poll_period_ms"];
        uint8_t new_upload_period_ms = doc["upload_period_ms"];
        uint8_t new_buffer_capacity = doc["buffer_capacity"];
        uint16_t new_reg_req_id_1 = doc["reg_req_id_1"];
        if (!validateConfig(new_poll_period_ms, new_upload_period_ms, new_buffer_capacity, new_reg_req_id_1)) 
        {
            return false;
        }else
        {
            updateConfig(new_poll_period_ms, new_upload_period_ms, new_buffer_capacity, new_reg_req_id_1);
            //update the flag to indicate config has changed
            config_changed = true;
            return true;

        }
    
    }
    
}

void ApplyConfig()
{
    ParamAddress paramAddress;
    uint8_t poll_period_ms = retrieveConfig(paramAddress.poll_period_ms_addr);
    uint8_t upload_period_ms = retrieveConfig(paramAddress.upload_period_ms_addr);
    uint8_t buffer_capacity = retrieveConfig(paramAddress.buffer_capacity_addr);
    uint16_t reg_req_id_1 = (retrieveConfig(paramAddress.reg_req_id_1_addr) << 8) | retrieveConfig(paramAddress.reg_req_id_1_addr + 1); 

    POLL_PERIOD_MS = poll_period_ms;
    UPLOAD_PERIOD_MS = upload_period_ms;
    BUFFER_CAPACITY = buffer_capacity;

    Serial.printf("Applied Config - Poll Period: %u ms, Upload Period: %u ms, Buffer Capacity: %u, Reg Req ID 1: %u\n",
                  POLL_PERIOD_MS, UPLOAD_PERIOD_MS, BUFFER_CAPACITY, reg_req_id_1);

    // Reset the config changed flag
    config_changed = false;
}