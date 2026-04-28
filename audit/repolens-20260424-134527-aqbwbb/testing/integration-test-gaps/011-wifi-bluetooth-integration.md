---
title: "[LOW] WiFi and Bluetooth controller integration lacks tests"
severity: LOW
domain: hardware
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:cdc_hal"
  - "area:wireless"
---

## Summary
WiFi and Bluetooth controller components provide wireless connectivity (vCard beacon, BLE serial), but **no integration tests** verify that wireless interfaces initialize and communicate correctly.

## Evidence

**IWifiController API** (`components/cdc_hal/include/cdc_hal/IWifiController.h`):
```cpp
class IWifiController {
    bool connect(const char* ssid, const char* password);
    void disconnect();
    bool is_connected();
    std::string get_ip();
};
```

**IBluetoothController API** (`components/cdc_hal/include/cdc_hal/IBluetoothController.h`):
```cpp
class IBluetoothController {
    bool startAdvertising();
    void stopAdvertising();
    bool is_connected();
};
```

**Implementation** (`components/cdc_hal/src/WifiController.cpp`, `BluetoothController.cpp`):
- WiFi using ESP-IDF WiFi component
- BLE using ESP-IDF NimBLE
- vCard beacon with BLE advertising

**Module integration** (`components/mod_vcard/src/VcardModule.cpp`, `mod_ble_serial/`):
- vCard module uses BLE for beacon
- BLE serial module provides serial-over-BLE
- No integration tests

**Usage in main** (`main/main.cpp:142-157`):
```cpp
wifiController = cdc::hal::getWifiControllerInstance();
wifiController->init();
wifiController->start();

btController = cdc::hal::getBluetoothControllerInstance();
btController->init();
btController->start();
```

**Current test coverage**: Only `test_ble_vcard_symbols` which just calls basic functions

## Impact
- **BLE advertising**: May not be discoverable by phones
- **vCard data**: BLE beacon payload may be incorrect
- **WiFi connection**: May not connect to networks
- **BLE serial**: Data may not transmit/receive correctly

## Recommended Fix

Create integration test `test_wireless_integration/` that verifies:

1. **BLE initialization**: BLE stack starts correctly
2. **BLE advertising**: vCard beacon advertises with correct data
3. **BLE connection**: Device connects to phone
4. **WiFi initialization**: WiFi stack starts correctly
5. **WiFi connection**: Connects to configured network

**Test structure** (example):
```cpp
// test/test_wireless_integration/test_ble_vcard.cpp
#include "mod_vcard/ble_vcard.h"

void test_ble_vcard_advertising() {
    ble_vcard_init();
    
    // Set vCard data
    vcard_store_set_own("BEGIN:VCARD...", strlen(...), ...);
    
    // Start advertising
    ble_vcard_start_advertising();
    
    // Verify advertising parameters (hardware-dependent test)
}
```

## References
- [IWifiController interface](components/cdc_hal/include/cdc_hal/IWifiController.h)
- [IBluetoothController interface](components/cdc_hal/include/cdc_hal/IBluetoothController.h)
- [WifiController implementation](components/cdc_hal/src/WifiController.cpp)
- [BluetoothController implementation](components/cdc_hal/src/BluetoothController.cpp)
- [BLE vCard](components/mod_vcard/src/ble_vcard.cpp)
