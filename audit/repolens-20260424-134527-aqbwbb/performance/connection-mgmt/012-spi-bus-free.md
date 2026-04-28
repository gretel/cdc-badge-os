---
title: "[MEDIUM] SPI bus initialized but never freed on shutdown"
severity: MEDIUM
domain: connection-mgmt
lens: performance/connection-mgmt
labels:
  - "audit:performance/connection-mgmt"
---

## Summary

In `components/cdc_hal/src/SpiBus.cpp`, the `initSharedSpiBus()` function calls `spi_bus_initialize()` to initialize the SPI bus, but there's no corresponding `spi_bus_free()` call in any shutdown path. The SPI bus is initialized once at boot and never freed, which is acceptable for a system that runs continuously but means the system cannot properly clean up if the SPI bus needs to be reinitialized.

**Location:** `components/cdc_hal/src/SpiBus.cpp:32-65`

## Impact

1. **No clean shutdown**: The SPI bus remains initialized throughout system runtime with no way to free it.

2. **Re-initialization impossible**: If the SPI bus needs to be reconfigured (e.g., different clock speed, different pins), there's no way to free and reinitialize it.

3. **Resource tracking unclear**: No way to know if the bus is already initialized without checking the atomic flag.

4. **Power management**: The SPI peripheral stays active even if not in use, preventing lowest power modes.

## Evidence

**File: `components/cdc_hal/src/SpiBus.cpp`**

1. **initSharedSpiBus() initializes but never frees (line 32-65):**
```cpp
bool initSharedSpiBus(gpio_num_t mosi, gpio_num_t miso, gpio_num_t sclk) {
    static std::atomic<bool> g_spiInitialized{false};
    
    bool expected = false;
    if (!g_spiInitialized.compare_exchange_strong(expected, true)) {
        return true;  // Already initialized
    }

    spi_bus_config_t bus_conf = {};
    bus_conf.mosi_io_num = mosi;
    bus_conf.miso_io_num = miso;
    bus_conf.sclk_io_num = sclk;
    bus_conf.quadwp_io_num = -1;
    bus_conf.quadhd_io_num = -1;
    bus_conf.max_transfer_sz = SPI_BUFFER_SIZE;

    esp_err_t err = spi_bus_initialize(SPI2_HOST, &bus_conf, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        g_spiInitialized.store(false);
        LOG_E(TAG, "SPI bus init failed: %s", esp_err_to_name(err));
        return false;
    }

    LOG_I(TAG, "Shared SPI bus initialized (MOSI=%d, MISO=%d, SCLK=%d)", 
          mosi, miso, sclk);
    return true;
}
```

2. **No corresponding free function:**
There is no `freeSharedSpiBus()` or similar function to call `spi_bus_free()`.

3. **SPI2_HOST is hardcoded:**
```cpp
esp_err_t err = spi_bus_initialize(SPI2_HOST, &bus_conf, SPI_DMA_CH_AUTO);
```
This means only SPI2_HOST can be initialized, with no way to free and reinitialize.

4. **ESP-IDF API requires matching calls:**
- `spi_bus_initialize()` creates the bus
- `spi_bus_free()` should be called to free the bus

## Recommended Fix

Add a function to free the SPI bus and call it during shutdown:

1. **Add free function:**
```cpp
// In SpiBus.h
void freeSharedSpiBus();

// In SpiBus.cpp
void freeSharedSpiBus() {
    static std::atomic<bool> g_spiInitialized{false};
    
    bool expected = true;
    if (!g_spiInitialized.compare_exchange_strong(expected, false)) {
        return;  // Not initialized or already freed
    }

    esp_err_t err = spi_bus_free(SPI2_HOST);
    if (err != ESP_OK) {
        LOG_W(TAG, "SPI bus free returned: %s", esp_err_to_name(err));
        // Reset flag anyway
        g_spiInitialized.store(false);
    } else {
        LOG_I(TAG, "Shared SPI bus freed");
    }
}
```

2. **Call in system shutdown (main.cpp):**
```cpp
// In main shutdown sequence
freeSharedSpiBus();
```

3. **Alternatively, use RAII for automatic cleanup:**
```cpp
class SpiBusGuard {
public:
    SpiBusGuard() {
        initSharedSpiBus(MOSI_PIN, MISO_PIN, SCLK_PIN);
    }
    
    ~SpiBusGuard() {
        freeSharedSpiBus();
    }
    
    static SpiBusGuard& instance() {
        static SpiBusGuard guard;
        return guard;
    }
    
private:
    SpiBusGuard() = default;
};
```

## References

- [ESP-IDF SPI Driver API](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/spi_master.html) - spi_bus_initialize and spi_bus_free
- [SPI Bus Lifecycle](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/spi_master.html#overview) - Proper initialization and cleanup

</content>