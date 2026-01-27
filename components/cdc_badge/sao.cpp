// SAO (Shitty Add-On) Binary Descriptor Parser
// Implements the badge.team SAO standard

#include "sao.h"
#include "hw_config.h"
#include "i2c_bus.h"
#include "cdc_log.h"
#include "driver/i2c.h"
#include <string.h>
#include <stdio.h>

static sao_descriptor_t g_sao_descriptor;
static bool g_sao_detected = false;
static bool g_sao_initialized = false;

// Read from SAO EEPROM at specified memory address
static esp_err_t sao_read_eeprom(uint8_t mem_addr, uint8_t *data, size_t len) {
    if (!data || len == 0) return ESP_ERR_INVALID_ARG;

    // EEPROM read: Write memory address, then read data
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SAO_EEPROM_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, mem_addr, true);

    // Repeated start for read
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SAO_EEPROM_ADDR << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, &data[len - 1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(I2C_NUM_1, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    return err;
}

// Parse SAO Binary Descriptor from EEPROM
static bool sao_parse_descriptor(sao_descriptor_t *desc) {
    if (!desc) return false;

    memset(desc, 0, sizeof(sao_descriptor_t));

    uint8_t buf[4];
    uint8_t offset = 0;

    // 1. Read and check magic bytes "LIFE" (tolerant: check "IFE" bytes 1-3)
    if (sao_read_eeprom(0x00, buf, 4) != ESP_OK) {
        LOG_D("SAO", "Failed to read magic bytes");
        return false;
    }

    if (buf[1] != 'I' || buf[2] != 'F' || buf[3] != 'E') {
        LOG_D("SAO", "Invalid magic: %02X %02X %02X %02X", buf[0], buf[1], buf[2], buf[3]);
        return false;
    }
    offset = 4;

    // 2. Read SAO name length and name
    uint8_t name_len;
    if (sao_read_eeprom(offset, &name_len, 1) != ESP_OK) return false;
    offset++;

    if (name_len > SAO_MAX_NAME_LEN) name_len = SAO_MAX_NAME_LEN;
    if (name_len > 0) {
        if (sao_read_eeprom(offset, (uint8_t*)desc->name, name_len) != ESP_OK) return false;
    }
    desc->name[name_len] = '\0';
    offset += name_len;

    // 3. Read primary driver name
    uint8_t drv_name_len;
    if (sao_read_eeprom(offset, &drv_name_len, 1) != ESP_OK) return false;
    offset++;

    if (drv_name_len > SAO_MAX_DRIVER_NAME_LEN) drv_name_len = SAO_MAX_DRIVER_NAME_LEN;
    if (drv_name_len > 0) {
        if (sao_read_eeprom(offset, (uint8_t*)desc->primary_driver.name, drv_name_len) != ESP_OK) return false;
    }
    desc->primary_driver.name[drv_name_len] = '\0';
    offset += drv_name_len;

    // 4. Read primary driver data
    uint8_t drv_data_len;
    if (sao_read_eeprom(offset, &drv_data_len, 1) != ESP_OK) return false;
    offset++;

    desc->primary_driver.data_len = (drv_data_len > SAO_MAX_DRIVER_DATA_LEN)
                                     ? SAO_MAX_DRIVER_DATA_LEN : drv_data_len;
    if (desc->primary_driver.data_len > 0) {
        if (sao_read_eeprom(offset, desc->primary_driver.data, desc->primary_driver.data_len) != ESP_OK) {
            return false;
        }
    }
    offset += drv_data_len;

    // 5. Read number of additional drivers
    uint8_t extra_count;
    if (sao_read_eeprom(offset, &extra_count, 1) != ESP_OK) return false;
    offset++;

    desc->extra_driver_count = (extra_count > SAO_MAX_EXTRA_DRIVERS)
                                ? SAO_MAX_EXTRA_DRIVERS : extra_count;

    // 6. Read additional drivers
    for (uint8_t i = 0; i < desc->extra_driver_count; i++) {
        // Driver name
        if (sao_read_eeprom(offset, &drv_name_len, 1) != ESP_OK) break;
        offset++;

        if (drv_name_len > SAO_MAX_DRIVER_NAME_LEN) drv_name_len = SAO_MAX_DRIVER_NAME_LEN;
        if (drv_name_len > 0) {
            if (sao_read_eeprom(offset, (uint8_t*)desc->extra_drivers[i].name, drv_name_len) != ESP_OK) break;
        }
        desc->extra_drivers[i].name[drv_name_len] = '\0';
        offset += drv_name_len;

        // Driver data
        if (sao_read_eeprom(offset, &drv_data_len, 1) != ESP_OK) break;
        offset++;

        desc->extra_drivers[i].data_len = (drv_data_len > SAO_MAX_DRIVER_DATA_LEN)
                                           ? SAO_MAX_DRIVER_DATA_LEN : drv_data_len;
        if (desc->extra_drivers[i].data_len > 0) {
            if (sao_read_eeprom(offset, desc->extra_drivers[i].data, desc->extra_drivers[i].data_len) != ESP_OK) {
                break;
            }
        }
        offset += drv_data_len;
    }

    desc->valid = true;
    return true;
}

bool sao_init(void) {
    if (g_sao_initialized) return true;

    memset(&g_sao_descriptor, 0, sizeof(g_sao_descriptor));
    g_sao_detected = false;
    g_sao_initialized = true;

    LOG_I("SAO", "SAO module initialized");
    return true;
}

bool sao_scan(void) {
    if (!g_sao_initialized) {
        sao_init();
    }

    // Reset state
    g_sao_detected = false;
    memset(&g_sao_descriptor, 0, sizeof(g_sao_descriptor));

    // Try to parse descriptor
    if (sao_parse_descriptor(&g_sao_descriptor)) {
        g_sao_detected = true;
        LOG_I("SAO", "SAO detected: %s (driver: %s)",
              g_sao_descriptor.name,
              g_sao_descriptor.primary_driver.name);
        return true;
    }

    LOG_D("SAO", "No SAO detected");
    return false;
}

bool sao_read_descriptor(sao_descriptor_t *desc) {
    if (!desc) return false;

    if (!g_sao_detected) {
        memset(desc, 0, sizeof(sao_descriptor_t));
        return false;
    }

    memcpy(desc, &g_sao_descriptor, sizeof(sao_descriptor_t));
    return true;
}

bool sao_is_detected(void) {
    return g_sao_detected;
}

const char* sao_get_name(void) {
    if (!g_sao_detected) return "";
    return g_sao_descriptor.name;
}

const char* sao_get_driver_name(void) {
    if (!g_sao_detected) return "";
    return g_sao_descriptor.primary_driver.name;
}

void sao_get_info_string(char *buf, size_t len) {
    if (!buf || len == 0) return;

    if (!g_sao_detected) {
        snprintf(buf, len, "No SAO detected\n\nConnect an SAO module\nto the SAO port and\nuse 'Scan' to detect it.");
        return;
    }

    int pos = 0;

    // Name
    pos += snprintf(buf + pos, len - pos, "Name: %s\n\n",
                    g_sao_descriptor.name[0] ? g_sao_descriptor.name : "(none)");

    // Primary driver
    if (pos < (int)len) {
        pos += snprintf(buf + pos, len - pos, "Driver: %s\n",
                        g_sao_descriptor.primary_driver.name[0]
                        ? g_sao_descriptor.primary_driver.name : "(none)");
    }

    // Driver data (hex)
    if (pos < (int)len && g_sao_descriptor.primary_driver.data_len > 0) {
        pos += snprintf(buf + pos, len - pos, "Data: ");
        for (int i = 0; i < g_sao_descriptor.primary_driver.data_len && pos < (int)len - 3; i++) {
            pos += snprintf(buf + pos, len - pos, "%02X ",
                            g_sao_descriptor.primary_driver.data[i]);
        }
        if (pos < (int)len) {
            buf[pos++] = '\n';
        }
    }

    // Extra drivers
    if (pos < (int)len && g_sao_descriptor.extra_driver_count > 0) {
        pos += snprintf(buf + pos, len - pos, "\nExtra drivers: %d\n",
                        g_sao_descriptor.extra_driver_count);
        for (int i = 0; i < g_sao_descriptor.extra_driver_count && pos < (int)len; i++) {
            pos += snprintf(buf + pos, len - pos, "  - %s\n",
                            g_sao_descriptor.extra_drivers[i].name);
        }
    }

    // Note about no driver support
    if (pos < (int)len) {
        pos += snprintf(buf + pos, len - pos, "\n(Basic detection only,\nno driver support)");
    }
}
