#pragma once

// I2C Bus abstraction for ESP-IDF
// Uses legacy i2c driver for stability
// Provides simplified access to the two I2C buses on the CDC Badge:
//   I2C0: BQ25895 charger + TCA9535 IO expander
//   I2C1: Expansion header (reserved)

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle types (compatible with legacy driver)
typedef void* i2c_master_bus_handle_t;
typedef void* i2c_master_dev_handle_t;

// Global bus handles
extern i2c_master_bus_handle_t I2C0_Bus;
extern i2c_master_bus_handle_t I2C1_Bus;

// Initialize both I2C buses
esp_err_t i2c_bus_init(void);

// Add a device to a bus
esp_err_t i2c_bus_add_device(i2c_master_bus_handle_t bus,
                             uint8_t addr,
                             i2c_master_dev_handle_t *out_dev);

// Single-register read/write helpers
esp_err_t i2c_write_reg(i2c_master_dev_handle_t dev,
                        uint8_t reg,
                        const uint8_t *data,
                        size_t len);

esp_err_t i2c_read_reg(i2c_master_dev_handle_t dev,
                       uint8_t reg,
                       uint8_t *data,
                       size_t len);

#ifdef __cplusplus
}
#endif
