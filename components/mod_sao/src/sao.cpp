/**
 * \brief SAO (Standardized Add-On) binary descriptor parser implementing badge.team SAO format.
 */

#include "mod_sao/sao.h"
#include "cdc_hal/II2cBus.h"
#include "cdc_log.h"
#include <cstring>

static const char* TAG = "SAO";

static sao_descriptor_t g_sao_descriptor;
static bool g_sao_detected = false;
static bool g_sao_initialized = false;
static cdc::hal::II2cBus* g_sao_bus = nullptr;
static cdc::hal::I2cDeviceHandle g_sao_dev = nullptr;

/**
 * \brief Reads bytes from SAO EEPROM at a given memory address.
 * \param mem_addr Start address in EEPROM.
 * \param data Output buffer receiving the bytes.
 * \param len Number of bytes to read.
 * \return ESP error code from the I2C read operation.
 */
static esp_err_t sao_read_eeprom(uint8_t mem_addr, uint8_t* data, size_t len) {
    if (!g_sao_bus || !g_sao_dev) return ESP_FAIL;
    return g_sao_bus->readReg(g_sao_dev, mem_addr, data, len);
}

/**
 * \brief Parses the SAO binary descriptor from EEPROM content.
 * \param desc Output descriptor structure.
 * \return `true` if parsing succeeded, otherwise `false`.
 */
static bool sao_parse_descriptor(sao_descriptor_t* desc) {
    if (!desc) return false;
    memset(desc, 0, sizeof(sao_descriptor_t));

    // 1. Read magic bytes
    uint8_t buf[4] = {0};
    if (sao_read_eeprom(0x00, buf, 4) != ESP_OK) {
        LOG_D(TAG, "Failed to read magic bytes");
        return false;
    }

    if (buf[0] != 0x53 || buf[1] != 0x41 || buf[2] != 0x4F || buf[3] != 0x31) {
        LOG_D(TAG, "Invalid magic: %02X %02X %02X %02X", buf[0], buf[1], buf[2], buf[3]);
        return false;
    }

    // 2. Read SAO name length and name
    uint8_t offset = 4;
    uint8_t name_len = 0;
    if (sao_read_eeprom(offset, &name_len, 1) != ESP_OK) return false;
    offset += 1;
    if (name_len > SAO_MAX_NAME_LEN) name_len = SAO_MAX_NAME_LEN;
    if (name_len > 0) {
        if (sao_read_eeprom(offset, reinterpret_cast<uint8_t*>(desc->name), name_len) != ESP_OK) return false;
        desc->name[name_len] = '\0';
        offset += name_len;
    }

    // 3. Primary driver name
    uint8_t drv_name_len = 0;
    if (sao_read_eeprom(offset, &drv_name_len, 1) != ESP_OK) return false;
    offset += 1;
    if (drv_name_len > SAO_MAX_DRIVER_NAME_LEN) drv_name_len = SAO_MAX_DRIVER_NAME_LEN;
    if (drv_name_len > 0) {
        if (sao_read_eeprom(offset, reinterpret_cast<uint8_t*>(desc->primary_driver.name), drv_name_len) != ESP_OK) {
            return false;
        }
        desc->primary_driver.name[drv_name_len] = '\0';
        offset += drv_name_len;
    }

    // 4. Primary driver data
    uint8_t drv_data_len = 0;
    if (sao_read_eeprom(offset, &drv_data_len, 1) != ESP_OK) return false;
    offset += 1;
    desc->primary_driver.data_len = (drv_data_len > SAO_MAX_DRIVER_DATA_LEN) ? SAO_MAX_DRIVER_DATA_LEN : drv_data_len;
    if (desc->primary_driver.data_len > 0) {
        if (sao_read_eeprom(offset, desc->primary_driver.data, desc->primary_driver.data_len) != ESP_OK) {
            return false;
        }
        offset += desc->primary_driver.data_len;
    }

    // 5. Extra drivers
    uint8_t extra_count = 0;
    if (sao_read_eeprom(offset, &extra_count, 1) != ESP_OK) return false;
    offset += 1;
    desc->extra_driver_count = (extra_count > SAO_MAX_EXTRA_DRIVERS) ? SAO_MAX_EXTRA_DRIVERS : extra_count;

    for (uint8_t i = 0; i < desc->extra_driver_count; i++) {
        if (sao_read_eeprom(offset, &drv_name_len, 1) != ESP_OK) break;
        offset += 1;
        if (drv_name_len > SAO_MAX_DRIVER_NAME_LEN) drv_name_len = SAO_MAX_DRIVER_NAME_LEN;
        if (drv_name_len > 0) {
            if (sao_read_eeprom(offset,
                                reinterpret_cast<uint8_t*>(desc->extra_drivers[i].name),
                                drv_name_len) != ESP_OK) {
                break;
            }
            desc->extra_drivers[i].name[drv_name_len] = '\0';
            offset += drv_name_len;
        }

        if (sao_read_eeprom(offset, &drv_data_len, 1) != ESP_OK) break;
        offset += 1;
        desc->extra_drivers[i].data_len =
            (drv_data_len > SAO_MAX_DRIVER_DATA_LEN) ? SAO_MAX_DRIVER_DATA_LEN : drv_data_len;
        if (desc->extra_drivers[i].data_len > 0) {
            if (sao_read_eeprom(offset, desc->extra_drivers[i].data,
                                desc->extra_drivers[i].data_len) != ESP_OK) {
                break;
            }
            offset += desc->extra_drivers[i].data_len;
        }
    }

    return true;
}

bool sao_init(void) {
    if (g_sao_initialized) return true;

    g_sao_bus = cdc::hal::getI2cBus1();
    if (!g_sao_bus) {
        LOG_E(TAG, "I2C1 bus not available");
        return false;
    }

    if (g_sao_bus->getState() == cdc::core::ServiceState::UNINITIALIZED) {
        if (!g_sao_bus->init()) {
            LOG_E(TAG, "I2C1 init failed");
            return false;
        }
    }
    if (g_sao_bus->getState() == cdc::core::ServiceState::INITIALIZED) {
        g_sao_bus->start();
    }

    if (g_sao_bus->addDevice(SAO_EEPROM_ADDR, &g_sao_dev) != ESP_OK) {
        LOG_E(TAG, "Failed to add SAO EEPROM device");
        return false;
    }

    memset(&g_sao_descriptor, 0, sizeof(g_sao_descriptor));
    g_sao_detected = false;
    g_sao_initialized = true;
    LOG_I(TAG, "SAO module initialized");
    return true;
}

bool sao_scan(void) {
    if (!g_sao_initialized) {
        if (!sao_init()) return false;
    }

    g_sao_detected = false;
    memset(&g_sao_descriptor, 0, sizeof(g_sao_descriptor));

    if (sao_parse_descriptor(&g_sao_descriptor)) {
        g_sao_detected = true;
        LOG_I(TAG, "SAO detected: %s (driver: %s)",
              g_sao_descriptor.name,
              g_sao_descriptor.primary_driver.name);
        return true;
    }

    LOG_D(TAG, "No SAO detected");
    return false;
}

bool sao_read_descriptor(sao_descriptor_t* desc) {
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

void sao_get_info_string(char* buf, size_t len) {
    if (!buf || len == 0) return;
    buf[0] = '\0';

    if (!g_sao_detected) {
        snprintf(buf, len,
                 "No SAO detected\n\nConnect an SAO module\nto the SAO port and\nunlock to detect it.");
        return;
    }

    int pos = 0;
    pos += snprintf(buf + pos, len - pos, "SAO: %s\n", g_sao_descriptor.name[0] ? g_sao_descriptor.name : "(none)");
    pos += snprintf(buf + pos, len - pos, "Driver: %s\n",
                    g_sao_descriptor.primary_driver.name[0]
                    ? g_sao_descriptor.primary_driver.name : "(none)");

    if (pos < static_cast<int>(len) && g_sao_descriptor.primary_driver.data_len > 0) {
        pos += snprintf(buf + pos, len - pos, "Driver Data:");
        for (int i = 0; i < g_sao_descriptor.primary_driver.data_len && pos < static_cast<int>(len) - 3; i++) {
            pos += snprintf(buf + pos, len - pos, " %02X", g_sao_descriptor.primary_driver.data[i]);
        }
        pos += snprintf(buf + pos, len - pos, "\n");
    }

    if (pos < static_cast<int>(len) && g_sao_descriptor.extra_driver_count > 0) {
        pos += snprintf(buf + pos, len - pos, "Extra Drivers: %d\n", g_sao_descriptor.extra_driver_count);
        for (int i = 0; i < g_sao_descriptor.extra_driver_count && pos < static_cast<int>(len); i++) {
            pos += snprintf(buf + pos, len - pos, " - %s\n", g_sao_descriptor.extra_drivers[i].name);
        }
    }
}
