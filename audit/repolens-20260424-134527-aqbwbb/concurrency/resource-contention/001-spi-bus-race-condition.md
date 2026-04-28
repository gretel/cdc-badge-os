---
title: "[HIGH] Shared SPI Bus Race Condition Between Display and TROPIC01"
severity: HIGH
domain: resource-contention
lens: concurrency
labels:
  - audit:concurrency/resource-contention
---

## Summary

The SPI bus (SPI2_HOST) is shared between the E-Paper display (CalEPD) and the TROPIC01 secure element, but there is **no mutex or semaphore protecting concurrent access** to this shared resource. The `SpiBus.cpp` uses an atomic flag `g_spiInitialized` for one-time initialization, but no locking mechanism exists for actual SPI transactions.

**Location**: `components/cdc_hal/src/SpiBus.cpp:18-67` and `components/cdc_hal/src/EpaperDisplay.cpp`

## Impact

**Resource Contention Risk**: When the display and TROPIC01 are accessed concurrently (e.g., UI rendering while performing secure-element operations), SPI transactions can interleave, causing:

1. **Data corruption**: Mixed command/data bytes from both devices
2. **Protocol violations**: TROPIC01 handshake failures, display frame buffer corruption
3. **Unpredictable behavior**: Intermittent failures that are hard to reproduce

**Evidence**:
- `SpiBus.cpp:18`: `static std::atomic<bool> g_spiInitialized{0};` - only tracks initialization, not access
- `EpaperDisplay.cpp:179`: `s_epd_display->update()` - long-running SPI operation (can take 100-500ms)
- `Tropic01Element.cpp`: All secure-element operations call `lock()` for the *secure element mutex*, but **not** for SPI access

## Evidence

**SPI Bus Initialization** (`components/cdc_hal/src/SpiBus.cpp:34-67`):
```cpp
static std::atomic<bool> g_spiInitialized{false};

esp_err_t initSharedSpiBus() {
    // Already initialized?
    if (g_spiInitialized.load()) {
        return ESP_OK;
    }
    // ... SPI bus setup ...
    g_spiInitialized.store(true);
    return ESP_OK;
}
```

**Display SPI Access** (`components/cdc_hal/src/EpaperDisplay.cpp:179`):
```cpp
static void renderTask(void* arg) {
    xSemaphoreTake(s_renderMutex, portMAX_DELAY);
    bool doFull = s_renderFull;
    s_renderPending = false;
    xSemaphoreGive(s_renderMutex);

    if (s_epd_display) {
        if (doFull) {
            s_epd_display->update();  // <-- Long SPI transaction, no SPI mutex!
        } else {
            s_epd_display->updateWindow(0, 0, HEIGHT, WIDTH, false);
        }
    }
}
```

**TROPIC01 SPI Access** (`components/cdc_hal/src/Tropic01Element.cpp:95-96`):
```cpp
void lock() { if (mutex_) xSemaphoreTakeRecursive(mutex_, portMAX_DELAY); }
void unlock() { if (mutex_) xSemaphoreGiveRecursive(mutex_); }
```
The TROPIC01 uses a mutex for internal state, but **does not protect the shared SPI bus**.

## Recommended Fix

1. **Create a shared SPI bus mutex** in `SpiBus.cpp`:
```cpp
// In SpiBus.cpp
static SemaphoreHandle_t s_spiMutex = nullptr;

esp_err_t initSharedSpiBus() {
    // ... existing init code ...
    if (!s_spiMutex) {
        s_spiMutex = xSemaphoreCreateMutex();
    }
    g_spiInitialized.store(true);
    return ESP_OK;
}

// Export for use by both display and TROPIC01
spi_host_device_t getSharedSpiHost();
esp_err_t initSharedSpiBus();
SemaphoreHandle_t getSharedSpiMutex();  // New function
```

2. **Wrap TROPIC01 SPI operations** with the shared mutex:
```cpp
// In Tropic01Element.cpp
void Tropic01Element::sessionStart() {
    auto* spiMutex = cdc::hal::getSharedSpiMutex();
    xSemaphoreTake(spiMutex, portMAX_DELAY);  // Lock SPI bus
    
    // ... existing session start code ...
    
    xSemaphoreGive(spiMutex);  // Unlock SPI bus
}
```

3. **Display already has a mutex** (`s_renderMutex`), but it should be the **same** shared SPI mutex:
```cpp
// In EpaperDisplay.cpp
static SemaphoreHandle_t s_renderMutex = nullptr;

// Change to use shared SPI mutex
static SemaphoreHandle_t s_sharedSpiMutex = nullptr;

// In start():
s_sharedSpiMutex = cdc::hal::getSharedSpiMutex();
```

## References

- ESP32-S3 Technical Reference Manual: SPI Peripheral (Chapter 30)
- libtropic documentation: SPI timing requirements
- CalEPD library: E-Paper display SPI protocol (needs 4-10ms per frame)
- FreeRTOS: Binary/Recursive mutex usage for shared resources
