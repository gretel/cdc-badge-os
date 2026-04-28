---
title: "[MEDIUM] WiFi event group created but never destroyed"
severity: MEDIUM
domain: connection-mgmt
lens: performance/connection-mgmt
labels:
  - "audit:performance/connection-mgmt"
---

## Summary

In `components/cdc_hal/src/WifiController.cpp`, the `init()` method creates a FreeRTOS event group using `xEventGroupCreate()` but the `stop()` method never destroys it using `vEventGroupDelete()`. The event group handle `eventGroup_` is stored but remains valid throughout the system lifetime even after the WiFi controller is stopped.

**Location:** `components/cdc_hal/src/WifiController.cpp:152` (creation) and `180-187` (stop method)

## Impact

1. **Memory leak**: The event group uses a small amount of heap memory (typically 20-40 bytes on ESP32 depending on config) that is never freed.

2. **Accumulated leaks**: If `init()`/`stop()` cycles occur multiple times (e.g., during testing, reconfiguration, or sleep/wake cycles), multiple event groups accumulate without being cleaned up.

3. **Stale event handling**: Even after `stop()` is called, the event group still exists and could receive bits set from event handlers if they haven't been fully unregistered yet, potentially causing confusion.

4. **Resource tracking unclear**: No way to know if the event group is valid without checking the service state.

## Evidence

**File: `components/cdc_hal/src/WifiController.cpp`**

1. **Event group created in init() (line 152-154):**
```cpp
// Create event group
eventGroup_ = xEventGroupCreate();  // Created
if (!eventGroup_) {
    LOG_E(TAG, "Failed to create event group");
    state_ = core::ServiceState::ERROR;
    return false;
}
```

2. **Event group used throughout (line 417, 428, 516, etc.):**
```cpp
xEventGroupClearBits(eventGroup_, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT | WIFI_GOT_IP_BIT);
EventBits_t bits = xEventGroupWaitBits(eventGroup_, ...);
xEventGroupSetBits(eventGroup_, WIFI_CONNECTED_BIT);
```

3. **stop() doesn't destroy event group (line 180-187):**
```cpp
void WifiController::stop() {
    if (state_ == core::ServiceState::STARTED) {
        if (enabled_) {
            disable();  // Calls deinitNetif() but doesn't delete event group
        }
        state_ = core::ServiceState::STOPPED;
        // eventGroup_ never destroyed!
    }
}
```

4. **disable() also doesn't clean up event group (line 348-378):**
```cpp
void WifiController::disable() {
    // ...
    esp_wifi_stop();
    esp_wifi_deinit();
    // Unregisters event handlers but doesn't delete event group
    deinitNetif();  // Destroys netif but not event group
    // ...
}
```

5. **ESP-IDF FreeRTOS API:**
- `xEventGroupCreate()` creates an event group
- `vEventGroupDelete()` should be called to destroy it

## Recommended Fix

Add `vEventGroupDelete()` call in the `stop()` method:

1. **Update stop() method:**
```cpp
void WifiController::stop() {
    if (state_ == core::ServiceState::STARTED) {
        if (enabled_) {
            disable();
        }
        state_ = core::ServiceState::STOPPED;
    }
    
    // Destroy event group if it was created
    if (eventGroup_) {
        vEventGroupDelete(eventGroup_);
        eventGroup_ = nullptr;
    }
    
    LOG_I(TAG, "WiFi controller stopped");
}
```

2. **Add guard in init() for re-initialization:**
```cpp
bool WifiController::init() {
    if (state_ != core::ServiceState::UNINITIALIZED) {
        return state_ == core::ServiceState::INITIALIZED ||
               state_ == core::ServiceState::STARTED;
    }
    
    instance_ = this;
    
    // Destroy old event group if exists
    if (eventGroup_) {
        vEventGroupDelete(eventGroup_);
        eventGroup_ = nullptr;
    }
    
    // Create event group
    eventGroup_ = xEventGroupCreate();
    if (!eventGroup_) {
        LOG_E(TAG, "Failed to create event group");
        state_ = core::ServiceState::ERROR;
        return false;
    }
    
    state_ = core::ServiceState::INITIALIZED;
    LOG_I(TAG, "WiFi controller initialized");
    return true;
}
```

3. **Update deinitNetif() to also clean up event group:**
Alternatively, move the event group cleanup into `deinitNetif()` since both are network-related resources:
```cpp
void WifiController::deinitNetif() {
    if (staNetif_) {
        esp_netif_destroy(staNetif_);
        staNetif_ = nullptr;
    }
    if (apNetif_) {
        esp_netif_destroy(apNetif_);
        apNetif_ = nullptr;
    }
    
    // Also destroy event group
    if (eventGroup_) {
        vEventGroupDelete(eventGroup_);
        eventGroup_ = nullptr;
    }
}
```

## References

- [FreeRTOS Event Groups API](https://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_RTOS/RTOS-API/FreeRTOS-Event-Groups-API.html) - vEventGroupDelete
- [ESP-IDF WiFi Lifecycle](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_wifi.html) - WiFi initialization and cleanup
- [Resource Acquisition Is Initialization](https://en.wikipedia.org/wiki/Resource_acquisition_is_initialization) - RAII pattern for C++

</content>