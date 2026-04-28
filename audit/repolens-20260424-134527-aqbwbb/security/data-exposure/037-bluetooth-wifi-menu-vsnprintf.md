---
title: "[LOW] Bluetooth and WiFi menu UIs use vsnprintf for status formatting"
severity: LOW
domain: cdc_os_ui
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The Bluetooth and WiFi menu UI components use `vsnprintf` to format status information for display. This can expose detailed connection information including device names, SSIDs, signal strengths, and connection states.

**Locations:**
- `components/cdc_os_ui/src/BluetoothMenuUi.cpp:237` - BLE status formatting
- `components/cdc_os_ui/src/WifiMenuUi.cpp:642` - WiFi status formatting

## Impact
- **Device enumeration**: BLE device names and addresses are formatted for display
- **Network exposure**: WiFi SSIDs and connection details are formatted
- **Signal strength leakage**: RSSI values reveal proximity to devices/networks
- **Connection state**: Detailed status reveals connection timing and states

## Evidence
File: `components/cdc_os_ui/src/BluetoothMenuUi.cpp:237`
```cpp
int written = vsnprintf(info + pos, BLE_STATUS_BUF_SIZE - pos, fmt, args);
```

File: `components/cdc_os_ui/src/WifiMenuUi.cpp:642`
```cpp
int written = vsnprintf(info + pos, WIFI_DETAILS_BUF_SIZE - pos, fmt, args);
```

These are used to format:
- BLE device names and addresses
- WiFi SSIDs and BSSID
- Signal strength (RSSI)
- Connection states and IP addresses

## Recommended Fix
1. Consider masking SSIDs (show first 4 chars + "...")
2. Round RSSI values to reduce precision
3. Document that connection details are exposed in UI

Example fix:
```cpp
// Mask SSID for privacy
void formatSSID(char* out, size_t outSize, const char* ssid) {
    if (strlen(ssid) > 4)
        snprintf(out, outSize, "%.4s...", ssid);
    else
        snprintf(out, outSize, "%s", ssid);
}
```

## References
- WiFi privacy: https://www.wi-fi.org/news-blog/privacy-and-wi-fi
- BLE scanning: https://www.bluetooth.com/bluetooth-resources/
- Related to issue #35 (HardwareInfo exposure)
