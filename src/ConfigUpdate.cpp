
#include "ConfigUpdate.h"
#include <EEPROM.h>
#include <ArduinoJson.h>
#include "Config.h"


static void updateConfig(uint16_t poll_period_ms, uint16_t upload_period_ms, uint16_t buffer_capacity, uint16_t reg_req_id_1) {

    ParamAddress paramAddress;

    // Update poll period
    EEPROM.write(paramAddress.poll_period_ms_addr, (poll_period_ms >> 8) & 0xFF); // High byte
    EEPROM.write(paramAddress.poll_period_ms_addr + 1, (poll_period_ms)& 0xFF); // Low byte

    // Update upload period
    EEPROM.write(paramAddress.upload_period_ms_addr, (upload_period_ms >> 8) & 0xFF); // High byte
    EEPROM.write(paramAddress.upload_period_ms_addr + 1, (upload_period_ms)& 0xFF); // Low byte

    // Update buffer capacity
    EEPROM.write(paramAddress.buffer_capacity_addr, (buffer_capacity >> 8) & 0xFF); // High byte
    EEPROM.write(paramAddress.buffer_capacity_addr + 1, (buffer_capacity)& 0xFF);     // Low byte

    // Update register request ID 1 (2 bytes)
    EEPROM.write(paramAddress.reg_req_id_1_addr, (reg_req_id_1 >> 8) & 0xFF); // High byte
    EEPROM.write(paramAddress.reg_req_id_1_addr + 1, (reg_req_id_1)& 0xFF);     // Low byte

    

    // Commit changes to EEPROM
    EEPROM.commit();
}

uint8_t retrieveConfig(uint8_t addr) {
    uint8_t value = EEPROM.read(addr);
    Serial.printf("Retrieving config from address: 0x%02X, Value: 0x%02X\n", addr, value);
    return value;
}


static bool validateConfig(uint16_t poll_period_ms, uint16_t upload_period_ms, uint16_t buffer_capacity, uint16_t reg_req_id_1) {
    // Example validation rules
    if (poll_period_ms < 100 || poll_period_ms > 30000) return false;
    if (upload_period_ms < 1000 || upload_period_ms > 300000) return false;
    if (buffer_capacity < 10 || buffer_capacity > 1000) return false;
    // reg_req_id_1 can be any 16-bit value
    
    return true;
}   

bool SaveConfig(byte* payload, unsigned int len) 
{
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, payload, len);
    Serial.println("Deserialize Json: " + String(error.c_str()));
    if (error) 
    {
        return false;
    }
    if (doc.containsKey("poll_period_ms") && doc.containsKey("upload_period_ms") &&
        doc.containsKey("buffer_capacity") && doc.containsKey("reg_req_id_1")) 
    {
        uint16_t new_poll_period_ms = doc["poll_period_ms"];
        uint16_t new_upload_period_ms = doc["upload_period_ms"];
        uint16_t new_buffer_capacity = doc["buffer_capacity"];
        uint16_t new_reg_req_id_1 = doc["reg_req_id_1"];
        

        Serial.printf("Poll Period (ms): %u\n", new_poll_period_ms);
        Serial.printf("Upload Period (ms): %u\n", new_upload_period_ms);
        Serial.printf("Buffer Capacity: %u\n", new_buffer_capacity);
        Serial.printf("Reg Req ID 1: %u\n", new_reg_req_id_1);


        if (!validateConfig(new_poll_period_ms, new_upload_period_ms, new_buffer_capacity, new_reg_req_id_1)) 
        {
            Serial.println("Config validation failed!");
            return false;
        }else
        {
            Serial.println("Config validation passed, updating EEPROM...");
            updateConfig(new_poll_period_ms, new_upload_period_ms, new_buffer_capacity, new_reg_req_id_1);
            //update the flag to indicate config has changed
            config_changed = true;
            return true;
        }
    } else {
        Serial.println("Missing required JSON keys in config!");
        return false;
    }
}

void ApplyConfig()
{
    ParamAddress paramAddress;
    uint16_t poll_period_ms = (retrieveConfig(paramAddress.poll_period_ms_addr) << 8) | retrieveConfig(paramAddress.poll_period_ms_addr + 1);
    uint16_t upload_period_ms = (retrieveConfig(paramAddress.upload_period_ms_addr) << 8) | retrieveConfig(paramAddress.upload_period_ms_addr + 1);
    uint16_t buffer_capacity = (retrieveConfig(paramAddress.buffer_capacity_addr) << 8) | retrieveConfig(paramAddress.buffer_capacity_addr + 1);
    uint16_t reg_req_id_1 = (retrieveConfig(paramAddress.reg_req_id_1_addr) << 8) | retrieveConfig(paramAddress.reg_req_id_1_addr + 1);

    POLL_PERIOD_MS = poll_period_ms;
    UPLOAD_PERIOD_MS = upload_period_ms;
    BUFFER_CAPACITY = buffer_capacity;
    REG_REQ_ID_1 = reg_req_id_1;
    Serial.printf("Applied Config - Poll Period: %u ms, Upload Period: %u ms, Buffer Capacity: %u, Reg Req ID 1: %u\n",
                  POLL_PERIOD_MS, UPLOAD_PERIOD_MS, BUFFER_CAPACITY, REG_REQ_ID_1);

    // Reset the config changed flag
    config_changed = false;

}