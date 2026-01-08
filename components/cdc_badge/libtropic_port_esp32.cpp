// ESP-IDF SPI port for libtropic (TROPIC01)
// Uses shared SPI bus with manual chip-select control

#include "libtropic_port_esp32.h"
#include "hw_config.h"
#include "spi_bus.h"
#include "libtropic_port.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cdc_log.h"

// Debug flag for raw SPI logging (0=off, 1=verbose hex dumps)
#ifndef TR01_SPI_DEBUG
#define TR01_SPI_DEBUG 0
#endif

// Helper to dump hex data
#if TR01_SPI_DEBUG
static void dump_hex(const char* prefix, const uint8_t* data, size_t len) {
    if (len == 0) return;
    char buf[256];
    size_t pos = 0;
    size_t max_bytes = (len > 32) ? 32 : len;  // Limit to 32 bytes
    for (size_t i = 0; i < max_bytes && pos < sizeof(buf) - 4; i++) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%02X ", data[i]);
    }
    if (len > 32) {
        snprintf(buf + pos, sizeof(buf) - pos, "...[%zu more]", len - 32);
    }
    LOG_D("TR01-SPI", "%s (%zu bytes): %s", prefix, len, buf);
}
#endif

extern "C" lt_ret_t lt_port_init(lt_l2_state_t *s2) {
    if (!s2 || !s2->device) {
        return LT_PARAM_ERR;
    }

    lt_dev_esp32_t *device = static_cast<lt_dev_esp32_t *>(s2->device);
    device->spi = nullptr;

    // Configure CS pin as output (manual control)
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = 1ULL << device->cs_pin;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(device->cs_pin, 1);

    // Initialize shared SPI bus
    if (spi_bus_init() != ESP_OK) {
        return LT_L1_SPI_ERROR;
    }

    // Add TROPIC01 as device on shared SPI bus
    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = 10 * 1000 * 1000;  // 10 MHz
    devcfg.mode = 0;
    devcfg.spics_io_num = -1;  // Manual CS
    devcfg.queue_size = 1;

    if (spi_bus_add_device(SPI_BUS_HOST, &devcfg, &device->spi) != ESP_OK) {
        return LT_L1_SPI_ERROR;
    }

    return LT_OK;
}

extern "C" lt_ret_t lt_port_deinit(lt_l2_state_t *s2) {
    if (!s2 || !s2->device) {
        return LT_PARAM_ERR;
    }

    lt_dev_esp32_t *device = static_cast<lt_dev_esp32_t *>(s2->device);
    if (device->spi) {
        spi_bus_remove_device(device->spi);
        device->spi = nullptr;
    }
    gpio_set_level(device->cs_pin, 1);
    return LT_OK;
}

extern "C" lt_ret_t lt_port_spi_csn_low(lt_l2_state_t *s2) {
    if (!s2 || !s2->device) {
        return LT_PARAM_ERR;
    }
    lt_dev_esp32_t *device = static_cast<lt_dev_esp32_t *>(s2->device);

    // Acquire exclusive SPI bus access before asserting CS
    // This prevents display from interfering during TROPIC01 operations
    if (device->spi) {
        esp_err_t err = spi_device_acquire_bus(device->spi, portMAX_DELAY);
        if (err != ESP_OK) {
            LOG_E("TR01-SPI", "Failed to acquire SPI bus: %d", err);
            return LT_L1_SPI_ERROR;
        }
    }

#if TR01_SPI_DEBUG
    LOG_D("TR01-SPI", "CS LOW (pin %d)", device->cs_pin);
#endif
    gpio_set_level(device->cs_pin, 0);
    return LT_OK;
}

extern "C" lt_ret_t lt_port_spi_csn_high(lt_l2_state_t *s2) {
    if (!s2 || !s2->device) {
        return LT_PARAM_ERR;
    }
    lt_dev_esp32_t *device = static_cast<lt_dev_esp32_t *>(s2->device);
#if TR01_SPI_DEBUG
    LOG_D("TR01-SPI", "CS HIGH (pin %d)", device->cs_pin);
#endif
    gpio_set_level(device->cs_pin, 1);

    // Release SPI bus after deasserting CS
    if (device->spi) {
        spi_device_release_bus(device->spi);
    }
    return LT_OK;
}

extern "C" lt_ret_t lt_port_spi_transfer(lt_l2_state_t *s2, uint8_t offset, uint16_t tx_len,
                                         uint32_t timeout_ms) {
    (void)timeout_ms;
    if (!s2 || !s2->device) {
        LOG_E("TR01-SPI", "spi_transfer: NULL s2 or device");
        return LT_PARAM_ERR;
    }
    lt_dev_esp32_t *device = static_cast<lt_dev_esp32_t *>(s2->device);
    if (!device->spi) {
        LOG_E("TR01-SPI", "spi_transfer: SPI not initialized");
        return LT_L1_SPI_ERROR;
    }

#if TR01_SPI_DEBUG
    // Log TX data before transfer
    dump_hex("TX", s2->buff + offset, tx_len);
#endif

    // In-place SPI transfer (TX/RX share buffer)
    spi_transaction_t t = {};
    t.length = static_cast<size_t>(tx_len) * 8;
    t.tx_buffer = s2->buff + offset;
    t.rx_buffer = s2->buff + offset;
    esp_err_t err = spi_device_polling_transmit(device->spi, &t);

#if TR01_SPI_DEBUG
    // Log RX data after transfer
    dump_hex("RX", s2->buff + offset, tx_len);

    // Check for suspicious patterns (all 0xFF = possible MISO issue)
    bool all_ff = true;
    for (uint16_t i = 0; i < tx_len && i < 8; i++) {
        if (s2->buff[offset + i] != 0xFF) {
            all_ff = false;
            break;
        }
    }
    if (all_ff && tx_len > 0) {
        LOG_W("TR01-SPI", "WARNING: RX data is all 0xFF - possible MISO/connection issue!");
    }
#endif

    if (err != ESP_OK) {
        LOG_E("TR01-SPI", "spi_device_polling_transmit failed: %d", err);
        return LT_L1_SPI_ERROR;
    }
    return LT_OK;
}

extern "C" lt_ret_t lt_port_delay(lt_l2_state_t *s2, uint32_t ms) {
    (void)s2;
    vTaskDelay(pdMS_TO_TICKS(ms));
    return LT_OK;
}

#if LT_USE_INT_PIN
extern "C" lt_ret_t lt_port_delay_on_int(lt_l2_state_t *s2, uint32_t ms) {
    (void)s2;
    (void)ms;
    return LT_L1_INT_TIMEOUT;
}
#endif

extern "C" lt_ret_t lt_port_random_bytes(lt_l2_state_t *s2, void *buff, size_t count) {
    (void)s2;
    if (!buff || count == 0) {
        return LT_PARAM_ERR;
    }
    esp_fill_random(buff, count);
    return LT_OK;
}
