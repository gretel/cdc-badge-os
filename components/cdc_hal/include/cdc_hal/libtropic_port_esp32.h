#pragma once

#include "driver/gpio.h"
#include "driver/spi_master.h"

// Device context for libtropic ESP32 port
typedef struct {
    spi_device_handle_t spi;
    gpio_num_t cs_pin;
} lt_dev_esp32_t;
