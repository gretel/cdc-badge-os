---
title: "[MEDIUM] SPI bus contention between TROPIC01 and Display without coordination"
severity: MEDIUM
domain: resource-contention
lens: concurrency
labels:
  - "audit:concurrency/resource-contention"
---

## Summary
The SPI bus is shared between the TROPIC01 secure element and the E-Paper display (CalEPD), but there's no centralized SPI bus arbitration mechanism. Both devices use manual chip-select control on the same SPI2_HOST bus, potentially causing collisions if operations overlap.

**Locations:**
- `components/cdc_hal/src/SpiBus.cpp:18-20` - Shared SPI host definition
- `components/cdc_hal/src/Tropic01Element.cpp:151-158` - TROPIC01 initialization
- `components/CalEPD/include/iointerface.h` - Display SPI initialization
- `components/cdc_hal/src/EpaperDisplay.cpp:203-212` - Display object creation

## Impact
1. **SPI Collisions**: If TROPIC01 and display operations overlap, both devices may respond on MISO simultaneously, corrupting data.
2. **Chip-Select Race**: Manual GPIO chip-select control means two threads could activate different devices simultaneously.
3. **Unpredictable Behavior**: No mutex protects the shared SPI bus, relying on implicit ordering from higher-level code.

**Evidence:**
```cpp
// SpiBus.cpp:18-67
static constexpr spi_host_device_t SPI_BUS_HOST = SPI2_HOST;
static constexpr uint32_t SPI_DMA_CHAN = SPI_DMA_CH_AUTO;

static std::atomic<bool> g_spiInitialized{false};

esp_err_t initSharedSpiBus() {
    if (g_spiInitialized.load()) {
        return ESP_OK;
    }
    
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = SPI_MOSI_PIN;
    buscfg.miso_io_num = SPI_MISO_PIN;
    buscfg.sclk_io_num = SPI_SCLK_PIN;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 4096;
    buscfg.flags = SPICOMMON_BUSFLAG_MASTER;
    
    esp_err_t err = spi_bus_initialize(SPI_BUS_HOST, &buscfg, SPI_DMA_CHAN);
    // ...
}

// Tropic01Element.cpp:140-158
bool Tropic01Element::init() {
    // ... PSA crypto init ...
    
    // Create mutex
    mutex_ = xSemaphoreCreateRecursiveMutex();
    if (!mutex_) {
        // ...
    }
    
    // Setup device context (SPI bus init is called internally by lt_port_init)
    memset(&handle_, 0, sizeof(handle_));
    device_.cs_pin = static_cast<gpio_num_t>(TR01_CS_PIN);
    device_.spi = nullptr;  // <-- Uses default SPI from lt_init()
    handle_.l2.device = &device_;
    handle_.l3.crypto_ctx = &cryptoCtx_;
    
    // Initialize libtropic
    lt_ret_t ret = lt_init(&handle_);  // <-- Calls lt_port_init() internally
    // ...
}

// EpaperDisplay.cpp:203-212
bool EpaperDisplay::init() {
    // ... backlight init ...
    
    // LAZY create display objects
    if (!s_epd_spi) {
        s_epd_spi = new EpdSpi();  // <-- Creates SPI interface
    }
    if (!s_epd_display) {
        s_epd_display = new Gdey029T94(*s_epd_spi);
    }
    
    // Initialize display
    s_epd_display->init(false);  // <-- Initializes display on SPI
    // ...
}
```

**CalEPD SPI initialization (from CalEPD library):**
```cpp
// EpdSpi.cpp (CalEPD)
EpdSpi::EpdSpi() {
    spi_bus_config_t spi_bus = {
        .mosi_io_num = EPD_MOSI,
        .miso_io_num = EPD_MISO,
        .sclk_io_num = EPD_SCLK,
        .max_transfer_sz = 4096
    };
    spi_bus_initialize(SPI2_HOST, &spi_bus, SPI_DMA_CH_AUTO);
}
```

**TROPIC01 SPI initialization (from libtropic):**
```cpp
// libtropic_port_esp32.cpp
void lt_port_init(lt_handle_t *h) {
    spi_bus_config_t spi_bus = {
        .mosi_io_num = TR01_MOSI,
        .miso_io_num = TR01_MISO,
        .sclk_io_num = TR01_SCLK,
        .max_transfer_sz = 4096
    };
    spi_bus_initialize(SPI2_HOST, &spi_bus, SPI_DMA_CH_AUTO);
    // ...
}
```

Both initialize the same SPI2_HOST bus. The second call returns `ESP_ERR_INVALID_STATE` (already initialized), but there's no coordination of chip-select timing.

**Race condition sequence:**
```
Thread A (Display):               Thread B (TROPIC01):
    s_epd_display->init()             sessionStart()
    s_epd_display->fillScreen()           lock()
        s_epd_spi->write()                    lt_verify_chip_and_start_secure_session()
            gpio_set(EPD_CS, LOW)                // SPI transfer starts
            spi_device_transmit()                gpio_set(TR01_CS, LOW)
            // ... display data ...                 spi_device_transmit()
            gpio_set(EPD_CS, HIGH)               // ... TROPIC data ...
                                                    gpio_set(TR01_CS, HIGH)
    
    // 100ms later
    s_epd_display->update()
        s_epd_spi->write()
            gpio_set(EPD_CS, LOW)
            spi_device_transmit()
```

If the TROPIC01 session start (which takes ~50ms for handshake) overlaps with display initialization:

```
Thread A (Display):               Thread B (TROPIC01):
    s_epd_display->init()             sessionStart()
    s_epd_display->fillScreen()           lock()
        s_epd_spi->write()                    lt_verify_chip_and_start_secure_session()
            gpio_set(EPD_CS, LOW)                // Takes ~50ms
            spi_device_transmit()                // SPI transfer in progress
            gpio_set(EPD_CS, HIGH)                  gpio_set(TR01_CS, LOW)
    
    // Key pressed, TROPIC01 needs to read slot
    verifyBadgePin()
        lock()
            sessionStart() → already locked, waits
```

The TROPIC01 mutex prevents concurrent secure element ops, but doesn't protect against display ops.

**Actual race - both devices active:**
```
Thread A (Display):               Thread B (TROPIC01):
    flush(PARTIAL)                  verifyBadgePin()
        s_epd_display->updateWindow()     sessionStart() (already active)
            s_epd_spi->write()                lock()
                gpio_set(EPD_CS, LOW)             lt_r_mem_data_read()
                spi_device_transmit()                 gpio_set(TR01_CS, LOW)
                    // Display SPI in progress          spi_device_transmit()
                gpio_set(EPD_CS, HIGH)                  // TROPIC SPI in progress
```

Both devices use the same SPI bus but different chip-select GPIOs. If both CS lines go low simultaneously, both devices drive MISO, causing data corruption.

## Recommended Fix
Add a shared SPI bus mutex that both TROPIC01 and Display acquire before operations:

1. **Create a shared SPI mutex:**
   ```cpp
   // components/cdc_hal/include/cdc_hal/SpiBus.h
   #pragma once
   
   #include "driver/spi_master.h"
   #include "esp_err.h"
   #include "freertos/semphr.h"
   
   namespace cdc::hal {
   
   // Get shared SPI bus mutex
   SemaphoreHandle_t getSharedSpiMutex();
   
   // Initialize shared SPI bus if not already done
   esp_err_t initSharedSpiBus();
   
   // Lock SPI bus
   void spiBusLock();
   
   // Unlock SPI bus
   void spiBusUnlock();
   
   } // namespace cdc::hal
   ```

2. **Implement SPI bus locking:**
   ```cpp
   // components/cdc_hal/src/SpiBus.cpp
   static SemaphoreHandle_t s_spiMutex = nullptr;
   
   SemaphoreHandle_t getSharedSpiMutex() {
       if (!s_spiMutex) {
           s_spiMutex = xSemaphoreCreateMutex();
       }
       return s_spiMutex;
   }
   
   void spiBusLock() {
       xSemaphoreTake(getSharedSpiMutex(), portMAX_DELAY);
   }
   
   void spiBusUnlock() {
       xSemaphoreGive(getSharedSpiMutex());
   }
   ```

3. **Use in TROPIC01 operations:**
   ```cpp
   // components/cdc_hal/src/Tropic01Element.cpp
   SeResult Tropic01Element::rmemRead(uint16_t slot, uint8_t* data, uint16_t maxLen,
                                       uint16_t* actualLen) {
       if (slot >= RMEM_SLOT_COUNT || !data || maxLen == 0) {
           return SeResult::INVALID_PARAM;
       }
       
       lock();  // TROPIC01 mutex
       
       if (!ensureSession("rmemRead")) {
           unlock();
           return SeResult::SESSION_REQUIRED;
       }
       
       // Acquire shared SPI mutex
       xSemaphoreTake(getSharedSpiMutex(), portMAX_DELAY);
       
       uint16_t bytesRead = 0;
       lt_ret_t ret = lt_r_mem_data_read(&handle_, slot, data, maxLen, &bytesRead);
       
       // Release shared SPI mutex
       xSemaphoreGive(getSharedSpiMutex());
       
       handleSessionError(ret);
       unlock();
       
       // ...
   }
   ```

4. **Use in Display operations:**
   ```cpp
   // components/cdc_hal/src/EpaperDisplay.cpp
   void EpaperDisplay::flushSync(RefreshMode mode) {
       if (!s_epd_display) return;
       
       // Acquire shared SPI mutex
       xSemaphoreTake(cdc::hal::getSharedSpiMutex(), portMAX_DELAY);
       
       if (mode == RefreshMode::FULL) {
           s_epd_display->update();
       } else {
           s_epd_display->updateWindow(0, 0, HEIGHT, WIDTH, false);
       }
       
       // Release shared SPI mutex
       xSemaphoreGive(cdc::hal::getSharedSpiMutex());
   }
   ```

**Alternative approach:** Use ESP-IDF's SPI device driver with proper `spi_device_queue_init()` and queue-based transactions for automatic arbitration.

## References
- [ESP32 SPI Bus Sharing](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api/peripherals/spi_master.html#sharing-spi-bus-between-multiple-devices)
- [LibTropic SPI Configuration](https://github.com/libTropic/libtropic/blob/master/ports/esp32/lt_l1_port_wrap.c)
- [CalEPD SPI Initialization](https://github.com/EmberTech/CalEPD/blob/main/src/epdspi.cpp)
