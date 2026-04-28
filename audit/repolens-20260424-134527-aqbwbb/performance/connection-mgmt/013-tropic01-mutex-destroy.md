---
title: "[LOW] TROPIC01 mutex created but never destroyed"
severity: LOW
domain: connection-mgmt
lens: performance/connection-mgmt
labels:
  - "audit:performance/connection-mgmt"
---

## Summary

In `components/cdc_hal/src/Tropic01Element.cpp`, a FreeRTOS mutex is created during `init()` using `xSemaphoreCreateMutex()` but is never destroyed in `stop()` or any cleanup path. The `mutex_` member holds the semaphore handle but there's no corresponding `vSemaphoreDelete()` call.

**Location:** `components/cdc_hal/src/Tropic01Element.cpp:132-134` (mutex creation) and `177-184` (stop method)

## Impact

1. **Memory leak**: The mutex uses a small amount of heap memory (typically 16-32 bytes on ESP32) that is never freed.

2. **Accumulated leaks**: If `init()`/`stop()` cycles occur multiple times (e.g., during testing or reconfiguration), multiple mutexes accumulate.

3. **Resource tracking**: The mutex handle is stored but never cleaned up, making it harder to track resource usage.

## Evidence

**File: `components/cdc_hal/src/Tropic01Element.cpp`**

1. **Mutex created in init() (line 132-134):**
```cpp
bool Tropic01Element::init() {
    // ...
    mutex_ = xSemaphoreCreateMutex();  // Created
    if (!mutex_) {
        LOG_E(TAG, "Failed to create mutex");
        return false;
    }
    // ...
}
```

2. **Mutex used throughout (line 97-98):**
```cpp
std::lock_guard<FreeRTOS::Mutex> lock(mutex_);  // Used for synchronization
```

3. **stop() doesn't destroy mutex (line 177-184):**
```cpp
void Tropic01Element::stop() {
    if (state_ == core::ServiceState::RUNNING) {
        sessionEnd();
        state_ = core::ServiceState::STOPPED;
        // mutex_ never destroyed!
        LOG_I(TAG, "TROPIC01 stopped");
    }
}
```

4. **ESP-IDF FreeRTOS API:**
- `xSemaphoreCreateMutex()` creates a mutex
- `vSemaphoreDelete()` should be called to destroy it

## Recommended Fix

Add `vSemaphoreDelete()` call in the `stop()` method:

1. **Update stop() method:**
```cpp
void Tropic01Element::stop() {
    if (state_ == core::ServiceState::RUNNING) {
        sessionEnd();
        state_ = core::ServiceState::STOPPED;
    }
    
    // Destroy mutex if it was created
    if (mutex_) {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }
    
    LOG_I(TAG, "TROPIC01 stopped");
}
```

2. **Add guard in init() for re-initialization:**
```cpp
bool Tropic01Element::init() {
    // Destroy old mutex if exists
    if (mutex_) {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
    }
    
    mutex_ = xSemaphoreCreateMutex();
    if (!mutex_) {
        LOG_E(TAG, "Failed to create mutex");
        return false;
    }
    // ...
}
```

## References

- [FreeRTOS Semaphore API](https://www.freertos.org/a00102.html) - vSemaphoreDelete
- [ESP-IDF FreeRTOS](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos.html) - Semaphore lifecycle

</content>