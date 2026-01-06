#pragma once

#include "driver/spi_master.h"
#include "esp_err.h"

// Shared SPI host for display + secure element
extern spi_host_device_t SPI_BUS_HOST;

// Initialize the shared SPI bus (ESP-IDF SPI master)
esp_err_t spi_bus_init(void);
