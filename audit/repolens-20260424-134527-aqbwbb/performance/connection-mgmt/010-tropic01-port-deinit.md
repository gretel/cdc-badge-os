---
title: "[HIGH] TROPIC01 secure element port deinit never called"
severity: HIGH
domain: connection-mgmt
lens: performance/connection-mgmt
labels:
  - "audit:performance/connection-mgmt"
---

## Summary

In `components/cdc_hal/src/Tropic01Element.cpp`, the `stop()` method calls `sessionEnd()` to end the secure element session but never calls `lt_port_deinit()` to properly deinitialize the SPI connection and release resources. The `lt_port_deinit()` function exists in `libtropic_port_esp32.cpp` but is only called from `init()`, not from any shutdown path.

**Location:** `components/cdc_hal/src/Tropic01Element.cpp:177-184` and `components/cdc_hal/src/libtropic_port_esp32.cpp:140-155`

## Impact

1. **SPI bus leak**: The SPI bus device added during init is never removed, potentially blocking other devices from using the SPI bus.

2. **GPIO leak**: The CS (chip select) pin configured during init remains configured but unused.

3. **Mutex leak**: The mutex created during init is never destroyed, leaking a small amount of heap memory.

4. **Session state inconsistency**: If the system tries to restart the secure element after calling `stop()`, the port might be in an undefined state.

5. **Power consumption**: The secure element might not enter its lowest power state if the SPI interface isn't properly torn down.

## Evidence

**File: `components/cdc_hal/src/Tropic01Element.cpp`**

1. **stop() method incomplete (line 177-184):**
```cpp
void Tropic01Element::stop() {
    if (state_ == core::ServiceState::RUNNING) {
        sessionEnd();  // Ends session but doesn't deinit port
        state_ = core::ServiceState::STOPPED;
        LOG_I(TAG, "TROPIC01 stopped");
    }
}
```

2. **init() calls port init (line 132-138):**
```cpp
bool Tropic01Element::init() {
    // ...
    if (spi_device_add_bus(&spi_cfg) == ESP_OK) {
        // ...
        if (lt_port_init(&handle_, &cs_pin, &spi, &device) == LT_OK) {  // Port initialized
            // ...
        }
    }
    // ...
}
```

**File: `components/cdc_hal/src/libtropic_port_esp32.cpp`**

3. **lt_port_deinit() exists but never called (line 140-155):**
```cpp
extern "C" void lt_port_deinit(lt_handle_t *h) {
    if (!h) return;

    lt_dev_esp32_t *device = static_cast<lt_dev_esp32_t *>(h->device);
    if (device) {
        if (device->spi) {
            spi_device_remove_device(device->spi);  // Removes SPI device
        }
        delete device;
    }
}
```

4. **lt_port_init() allocates resources (line 130-139):**
```cpp
extern "C" lt_ret_t lt_port_init(lt_handle_t *h, gpio_num_t *cs_pin,
                                  spi_device_handle_t *spi, void **device) {
    // ...
    *device = new lt_dev_esp32_t();  // Allocates device
    // ...
    spi_device_add_bus(&spi_cfg, &device->spi);  // Adds SPI device
    // ...
}
```

**File: `main/main.cpp`**

5. **Shutdown sequence doesn't call deinit:**
```cpp
// In main.cpp shutdown sequence:
if (s_secureElement) {
    s_secureElement->stop();  // Calls sessionEnd() but not port deinit
    // ...
}
// lt_port_deinit() never called
```

## Recommended Fix

Add a call to `lt_port_deinit()` in the `stop()` method:

1. **Update stop() to call port deinit:**
```cpp
void Tropic01Element::stop() {
    if (state_ == core::ServiceState::RUNNING) {
        sessionEnd();  // End session first
        
        // Deinitialize port (SPI, GPIO, mutex)
        lt_port_deinit(&handle_);
        
        state_ = core::ServiceState::STOPPED;
        LOG_I(TAG, "TROPIC01 stopped");
    }
}
```

2. **Add guard in deinit to handle multiple calls:**
```cpp
extern "C" void lt_port_deinit(lt_handle_t *h) {
    if (!h) return;
    
    lt_dev_esp32_t *device = static_cast<lt_dev_esp32_t *>(h->device);
    if (device) {
        if (device->spi) {
            spi_device_remove_device(device->spi);
            device->spi = nullptr;
        }
        delete device;
        h->device = nullptr;
    }
}
```

3. **Alternatively, add a wrapper method:**
```cpp
// In Tropic01Element.h
bool deinitPort();

// In Tropic01Element.cpp
bool Tropic01Element::deinitPort() {
    lt_port_deinit(&handle_);
    return true;
}

// In stop()
void Tropic01Element::stop() {
    if (state_ == core::ServiceState::RUNNING) {
        sessionEnd();
        deinitPort();  // Call wrapper
        state_ = core::ServiceState::STOPPED;
        LOG_I(TAG, "TROPIC01 stopped");
    }
}
```

## References

- [TROPIC01 Driver API](https://github.com/MicrochipTech/libtropic) - Port init/deinit
- [ESP-IDF SPI Driver](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/spi_master.html) - SPI device lifecycle
- [Resource Acquisition Is Initialization](https://en.wikipedia.org/wiki/Resource_acquisition_is_initialization) - RAII pattern for C++

</content>