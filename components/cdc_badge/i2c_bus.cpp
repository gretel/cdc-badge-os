// I2C Bus implementation for ESP-IDF
// Uses the legacy i2c driver API (more stable than new i2c_master)

#include "i2c_bus.h"
#include "hw_config.h"
#include "cdc_log.h"
#include "driver/i2c.h"
#include <string.h>

// We use port numbers directly instead of bus handles for legacy API
i2c_master_bus_handle_t I2C0_Bus = (i2c_master_bus_handle_t)1;  // Non-null marker
i2c_master_bus_handle_t I2C1_Bus = (i2c_master_bus_handle_t)2;  // Non-null marker

// Device handle stores: port (high byte) + address (low byte)
#define MAKE_DEV_HANDLE(port, addr) ((i2c_master_dev_handle_t)(uintptr_t)(((port) << 8) | (addr)))
#define GET_PORT(dev) ((i2c_port_t)(((uintptr_t)(dev) >> 8) & 0xFF))
#define GET_ADDR(dev) ((uint8_t)((uintptr_t)(dev) & 0xFF))

static bool i2c0_initialized = false;
static bool i2c1_initialized = false;

esp_err_t i2c_bus_init(void) {
    LOG_I("I2C", "i2c_bus_init() - using legacy driver");

    // I2C0: Charger (BQ25895) + IO Expander (TCA9535)
    i2c_config_t conf0 = {};
    conf0.mode = I2C_MODE_MASTER;
    conf0.sda_io_num = I2C0_SDA_PIN;
    conf0.scl_io_num = I2C0_SCL_PIN;
    conf0.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf0.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf0.master.clk_speed = 100000;  // 100kHz

    esp_err_t err = i2c_param_config(I2C_NUM_0, &conf0);
    if (err != ESP_OK) {
        LOG_E("I2C", "Failed to config I2C0: %d", err);
        return err;
    }

    err = i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) {
        LOG_E("I2C", "Failed to install I2C0 driver: %d", err);
        return err;
    }
    i2c0_initialized = true;

    // I2C1: Expansion header
    i2c_config_t conf1 = {};
    conf1.mode = I2C_MODE_MASTER;
    conf1.sda_io_num = I2C1_SDA_PIN;
    conf1.scl_io_num = I2C1_SCL_PIN;
    conf1.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf1.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf1.master.clk_speed = 100000;  // 100kHz

    err = i2c_param_config(I2C_NUM_1, &conf1);
    if (err != ESP_OK) {
        LOG_E("I2C", "Failed to config I2C1: %d", err);
        return err;
    }

    err = i2c_driver_install(I2C_NUM_1, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK) {
        LOG_E("I2C", "Failed to install I2C1 driver: %d", err);
        return err;
    }
    i2c1_initialized = true;

    LOG_I("I2C", "I2C buses initialized (legacy driver)");
    return ESP_OK;
}

esp_err_t i2c_bus_add_device(i2c_master_bus_handle_t bus,
                             uint8_t addr,
                             i2c_master_dev_handle_t *out_dev) {
    if (!bus || !out_dev) return ESP_ERR_INVALID_ARG;

    // Determine port from bus handle
    i2c_port_t port = (bus == I2C0_Bus) ? I2C_NUM_0 : I2C_NUM_1;

    // Create a fake device handle that encodes port + address
    *out_dev = MAKE_DEV_HANDLE(port, addr);

    return ESP_OK;
}

esp_err_t i2c_write_reg(i2c_master_dev_handle_t dev,
                        uint8_t reg,
                        const uint8_t *data,
                        size_t len) {
    if (!dev) return ESP_ERR_INVALID_ARG;

    i2c_port_t port = GET_PORT(dev);
    uint8_t addr = GET_ADDR(dev);

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    if (len > 0 && data) {
        i2c_master_write(cmd, data, len, true);
    }
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    return err;
}

esp_err_t i2c_read_reg(i2c_master_dev_handle_t dev,
                       uint8_t reg,
                       uint8_t *data,
                       size_t len) {
    if (!dev || !data || !len) return ESP_ERR_INVALID_ARG;

    i2c_port_t port = GET_PORT(dev);
    uint8_t addr = GET_ADDR(dev);

    // Write register address
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);

    // Repeated start and read
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, &data[len - 1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t err = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);

    return err;
}
