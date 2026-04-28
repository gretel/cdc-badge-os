---
title: "[LOW] WifiMenuUi and WiFi handlers integration lacks tests"
severity: LOW
domain: hardware
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:cdc_os_ui"
  - "area:wifi"
---

## Summary
The `WifiMenuUi` and `WifiHandlers` components (`components/cdc_os_ui/src/WifiMenuUi.cpp`, `WifiHandlers.cpp`) handle WiFi connection flow, but **no integration tests** verify that WiFi connects correctly and state is managed properly.

## Impact
- **Connection**: WiFi may not connect correctly
- **Scan**: Scan may not return results
- **State**: Connected/disconnected state may be wrong
- **Handlers**: WiFi events may not update UI

## Evidence

**WifiMenuUi API** (`components/cdc_os_ui/src/WifiMenuUi.cpp`):
```cpp
void showWifiMenu();
void onWifiConnect();
void onWifiScan();
```

**WifiHandlers API** (`components/cdc_os_ui/src/WifiHandlers.cpp`):
```cpp
void wifiScan();
void wifiConnect(const char* ssid, const char* password);
void wifiDisconnect();
bool wifiIsConnected();
std::string wifiGetIp();
```

**WiFi integration** (`components/cdc_hal/src/WifiController.cpp`):
```cpp
class IWifiController {
    bool connect(const char* ssid, const char* password);
    void disconnect();
    bool is_connected();
    std::string get_ip();
};
```

**Usage in AppUi** (`components/cdc_os_ui/src/AppUi.cpp:100-200`):
```cpp
// WiFi menu integrated into main menu
void showWifiMenu() {
    WifiMenuUi::show();
}
```

**Current test coverage**: None

## Recommended Fix

Create integration test `test_wifi_handlers/` that verifies:

1. **Scan**: WiFi scan returns results
2. **Connect**: Connect to network succeeds
3. **Disconnect**: Disconnect works
4. **State**: Connected state correct
5. **IP**: IP address retrieved

**Test structure** (example):
```cpp
// test/test_wifi_handlers/test_wifi.cpp
#include "cdc_os_ui/WifiHandlers.h"

void test_wifi_scan() {
    wifiScan();
    
    // Wait for scan complete
    // Verify results available
}

void test_wifi_connect() {
    wifiConnect("SSID", "password");
    
    // Wait for connection
    // Verify connected
    ASSERT_TRUE(wifiIsConnected());
}

void test_wifi_disconnect() {
    wifiConnect("SSID", "password");
    wifiDisconnect();
    
    ASSERT_FALSE(wifiIsConnected());
}
```

## References
- [WifiMenuUi implementation](components/cdc_os_ui/src/WifiMenuUi.cpp)
- [WifiHandlers](components/cdc_os_ui/src/WifiHandlers.cpp)
- [WifiController](components/cdc_hal/src/WifiController.cpp)

</content>