// Shared SPI bus initialization for display + TROPIC01
// Note: CalEPD initializes its own SPI, this handles TROPIC01 sharing

#include "spi_bus.h"
#include "hw_config.h"
#include "cdc_log.h"
#include "esp_err.h"

spi_host_device_t SPI_BUS_HOST = SPI2_HOST;

esp_err_t spi_bus_init(void) {
    LOG_I("SPI", "spi_bus_init()");

    spi_bus_config_t cfg = {};
    cfg.mosi_io_num = SPI_MOSI_PIN;
    cfg.miso_io_num = SPI_MISO_PIN;
    cfg.sclk_io_num = SPI_SCLK_PIN;
    cfg.quadwp_io_num = -1;
    cfg.quadhd_io_num = -1;
    cfg.max_transfer_sz = 4096;

    esp_err_t err = spi_bus_initialize(SPI_BUS_HOST, &cfg, SPI_DMA_CH_AUTO);
    if (err == ESP_ERR_INVALID_STATE) {
        // Bus already initialized (e.g., by CalEPD)
        LOG_I("SPI", "SPI bus already initialized");
        err = ESP_OK;
    }
    if (err != ESP_OK) {
        LOG_E("SPI", "SPI bus init failed (err=%d)", err);
    }
    return err;
}
