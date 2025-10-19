#pragma once
#include <Arduino.h>
#include "Poller.h"

extern Poller *g_poller;

/**
 * @brief Structure defining EEPROM memory addresses for configuration parameters
 * 
 * This structure maps configuration parameters to their corresponding EEPROM addresses.
 * Each parameter occupies 2 bytes of memory.
 */
typedef struct {
    uint8_t poll_period_ms_addr = 0x00;      ///< Polling period address (0x00-0x01, 2 bytes)
    uint8_t upload_period_ms_addr = 0x02;    ///< Upload period address (0x02-0x03, 2 bytes)  
    uint8_t buffer_capacity_addr = 0x04;     ///< Buffer capacity address (0x04-0x05, 2 bytes)
    uint8_t reg_req_id_1_addr = 0x06;        ///< Register request ID address (0x06-0x07, 2 bytes)
} ParamAddress;

/**
 * @brief Enumeration of error codes for configuration operations
 * 
 * Defines all possible error states that can occur during configuration
 * save, load, and validation operations.
 */
enum ErrorCode {
    ERR_OK = 0,                ///< No error occurred
    ERR_POLL_MS_FAILED,        ///< Polling period update failed
    ERR_UPLOAD_MS_FAILED,      ///< Upload period update failed
    ERR_BUFFER_CAPACITY_FAILED, ///< Buffer capacity update failed
    ERR_REG_REQ_ID_1_FAILED,  ///< Register request ID 1 update failed
    ERR_DESERIALIZE_FAILED,    ///< JSON deserialization failed
    ERR_UNKNOWN                ///< Unknown or unspecified error
};


extern bool config_changed;
extern bool writecommandreceived;
extern bool writeemulationreceived;

/**
 * @brief Retrieves a configuration value from EEPROM
 * 
 * @param addr The EEPROM address to read from
 * @return uint8_t The byte value stored at the specified address
 */
uint8_t retrieveConfig(uint8_t addr);



//static bool validateConfig(uint16_t poll_period_ms, uint16_t upload_period_ms, uint16_t buffer_capacity, uint16_t reg_req_id_1);

ErrorCode SaveConfig(byte* payload, unsigned int len);

/**
 * @brief Applies the saved configuration to the system
 * 
 * Reads configuration values from EEPROM and updates the running system
 * parameters accordingly. Should be called after SaveConfig() or on startup.
 */
void ApplyConfig();