#pragma once

// SAO (Shitty Add-On) Binary Descriptor Parser
// Implements the badge.team SAO standard:
// https://badge.team/docs/standards/sao/binary_descriptor/
//
// SAO modules have an EEPROM at I2C address 0x50 containing a binary
// descriptor with name, driver info, and optional driver data.

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// SAO Binary Descriptor Constants
#define SAO_EEPROM_ADDR         0x50
#define SAO_MAX_NAME_LEN        63
#define SAO_MAX_DRIVER_NAME_LEN 31
#define SAO_MAX_DRIVER_DATA_LEN 64
#define SAO_MAX_EXTRA_DRIVERS   4

// SAO Driver Information
typedef struct {
    char name[SAO_MAX_DRIVER_NAME_LEN + 1];
    uint8_t data[SAO_MAX_DRIVER_DATA_LEN];
    uint8_t data_len;
} sao_driver_info_t;

// SAO Descriptor (parsed from EEPROM)
typedef struct {
    bool valid;                                         // Descriptor successfully parsed
    char name[SAO_MAX_NAME_LEN + 1];                    // SAO Name
    sao_driver_info_t primary_driver;                   // Primary driver
    sao_driver_info_t extra_drivers[SAO_MAX_EXTRA_DRIVERS];
    uint8_t extra_driver_count;                         // Number of additional drivers
} sao_descriptor_t;

// Initialize SAO module (call after i2c_bus_init)
bool sao_init(void);

// Scan for SAO device and parse descriptor
// Returns true if SAO with valid descriptor found
bool sao_scan(void);

// Get the parsed descriptor (only valid if sao_scan returned true)
bool sao_read_descriptor(sao_descriptor_t *desc);

// Quick check if SAO is currently detected
bool sao_is_detected(void);

// Get SAO name (empty string if not detected)
const char* sao_get_name(void);

// Get primary driver name (empty string if not detected)
const char* sao_get_driver_name(void);

// Generate info string for display (includes name, driver, data)
void sao_get_info_string(char *buf, size_t len);

#ifdef __cplusplus
}
#endif
