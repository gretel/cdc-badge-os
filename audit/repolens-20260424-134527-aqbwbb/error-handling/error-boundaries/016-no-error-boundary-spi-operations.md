---
title: "[LOW] No error boundary around SPI bus operations"
severity: LOW
domain: error-handling
lens: error-boundaries
labels:
  - "error-handling"
  - "hardware"
---

## Summary
The `initSharedSpiBus()` function in `components/cdc_hal/src/SpiBus.cpp:33-65` initializes the SPI bus without comprehensive error handling. When the SPI bus initialization fails, callers must check return codes everywhere, leading to inconsistent error handling across the codebase.

**Evidence:**
- `components/cdc_hal/src/SpiBus.cpp:33-65`:
```cpp
esp_err_t initSharedSpiBus() {
    // Already initialized?
    if (g_spiInitialized.load()) {
        return ESP_OK;
    }

    // Configure SPI bus
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = SPI_MOSI_PIN;
    buscfg.miso_io_num = SPI_MISO_PIN;
    buscfg.sclk_io_num = SPI_SCLK_PIN;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 4096;
    buscfg.flags = SPICOMMON_BUSFLAG_MASTER;

    esp_err_t err = spi_bus_initialize(SPI_BUS_HOST, &buscfg, SPI_DMA_CHAN);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        LOG_E(TAG, "SPI bus init failed: %d", err);
        return err;  // Error returned but no boundary for callers
    }

    if (err == ESP_ERR_INVALID_STATE) {
        // Already initialized (by CalEPD most likely)
        LOG_I(TAG, "SPI bus already initialized (by display)");
    } else {
        LOG_I(TAG, "SPI bus initialized (MOSI=%d, MISO=%d, CLK=%d)",
                 SPI_MOSI_PIN, SPI_MISO_PIN, SPI_SCLK_PIN);
    }

    g_spiInitialized.store(true);
    return ESP_OK;
}
```

- Called from TROPIC01 element initialization (`components/cdc_hal/src/Tropic01Element.cpp:150`) with no retry logic.

## Impact
- **Error propagation**: Callers must check return codes everywhere
- **Inconsistent handling**: Some callers may ignore errors
- **No retry logic**: Transient SPI errors not recovered
- **Hardware failures**: SPI bus errors can cascade to display and secure element

## Recommended Fix
Add wrapper methods with error boundaries and optional retry:

```cpp
class SpiBus {
public:
    // Existing method
    esp_err_t initSharedSpiBus() override;

    // New wrapper with retry
    esp_err_t initSharedSpiBusWithRetry(uint8_t retries = 3) {
        for (uint8_t i = 0; i < retries; i++) {
            esp_err_t err = initSharedSpiBus();
            if (err == ESP_OK) return ESP_OK;
            if (err == ESP_ERR_INVALID_STATE) return ESP_OK;  // Already initialized
            vTaskDelay(pdMS_TO_TICKS(10));  // Brief delay before retry
        }
        LOG_E(TAG, "SPI bus init failed after %d retries", retries);
        return ESP_FAIL;
    }
};
```

Also add error boundary in TROPIC01 initialization:
```cpp
bool Tropic01Element::init() {
    if (state_ != core::ServiceState::UNINITIALIZED) {
        return state_ == core::ServiceState::INITIALIZED ||
               state_ == core::ServiceState::STARTED;
    }

    LOG_I(TAG, "Initializing TROPIC01...");

    // Initialize SPI bus with retry
    esp_err_t spiErr = initSharedSpiBus();
    if (spiErr != ESP_OK && spiErr != ESP_ERR_INVALID_STATE) {
        LOG_E(TAG, "SPI bus init failed: %d", spiErr);
        state_ = core::ServiceState::ERROR;
        return false;
    }

    // ... (rest of initialization)
}
```

## References
- [SPI Error Handling](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/spi_master.html)
- [Retry Pattern](https://learn.microsoft.com/en-us/azure/architecture/best-practices/retry-service-specific)
