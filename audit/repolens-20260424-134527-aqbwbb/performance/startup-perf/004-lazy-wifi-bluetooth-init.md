---
title: "[LOW] WiFi and Bluetooth initialize before user interaction"
severity: LOW
domain: startup-performance
lens: startup-perf
labels:
  - "audit:performance/startup-perf"
---

## Summary
WiFi (`main.cpp:138-144`) and Bluetooth (`main.cpp:147-153`) controllers are initialized and started **unconditionally** during boot, even though they may not be used immediately. These wireless stacks consume significant initialization time and power.

**Location:** `main/main.cpp:138-153`

## Impact
- **Startup delay**: WiFi init (~100-200ms) + Bluetooth init (~50-100ms) = 150-300ms added
- **Power consumption**: Both radios consume power during initialization even if idle
- **Memory usage**: WiFi stack (~30KB RAM) + BT stack (~20KB RAM) allocated upfront
- **User experience**: User waits for wireless init even if they just want to check TOTP

## Evidence
From `main/main.cpp:138-153`:
```cpp
// === WIFI CONTROLLER INITIALIZATION ===
LOG_I(TAG, "Initializing WiFi Controller...");
cdc::hal::IWifiController* wifiController = cdc::hal::getWifiControllerInstance();
if (wifiController && wifiController->init() && wifiController->start()) {
    LOG_I(TAG, "WiFi Controller ready");
}

// === BLUETOOTH CONTROLLER INITIALIZATION ===
LOG_I(TAG, "Initializing Bluetooth Controller...");
cdc::hal::IBluetoothController* btController = cdc::hal::getBluetoothControllerInstance();
if (btController && btController->init() && btController->start()) {
    LOG_I(TAG, "Bluetooth Controller ready");
}
```

From `components/cdc_hal/src/WifiController.cpp:148-190`:
```cpp
bool WifiController::init() {
    instance_ = this;
    eventGroup_ = xSemaphoreCreateBinary();  // FreeRTOS alloc
    // ...
    esp_netif_init();  // Network stack init
    ESP_ERROR_CHECK(esp_event_loop_init(wifiEventHandler, NULL));
    esp_wifi_init(&wifi_config);  // WiFi stack init
    // ...
}

bool WifiController::start() {
    esp_wifi_start();  // Radio starts scanning
    // ...
}
```

## Recommended Fix
**Lazy initialization**: Initialize WiFi/Bluetooth only when first accessed.

**Implementation:**
1. Add lazy getter methods to HAL interfaces:
```cpp
class IWifiController {
    static IWifiController* getInstance();  // Singleton
    IWifiController* getLazyInstance() {
        if (!instance_) {
            instance_ = new WifiController();
            instance_->init();  // First use
        }
        return instance_;
    }
};
```

2. Modify modules to use lazy getters:
```cpp
// In mod_ble_serial, mod_fido2 (for BLE transport)
auto* bt = getBluetoothControllerInstance();  // Lazy init
if (bt) {
    bt->start();
}
```

3. Keep current behavior for modules that require these at boot (e.g., BLE serial mode)

**Alternative**: Feature flag-based init
```cpp
#ifdef WIFI_ENABLED
    // Initialize WiFi
#endif
```

## References
- ESP-IDF WiFi: [Initialization overhead](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/network/esp_wifi.html)
- ESP-IDF BLE: [Bluetooth stack size](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/bluetooth/bluedroid.html)
