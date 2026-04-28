---
title: "[MEDIUM] WiFi scan result allocation failure returns empty results without indication"
severity: MEDIUM
domain: graceful-degradation
lens: error-handling
labels:
  - "wifi"
  - "memory"
  - "graceful-degradation"
---

## Summary
In `components/cdc_hal/src/WifiController.cpp:547-598`, the `getScanResults()` method allocates a dynamic buffer to hold WiFi scan results. When memory allocation fails (line 565), the function returns `0` without distinguishing between "no results found" and "out of memory". This causes callers to treat OOM as "empty scan" rather than a recoverable error.

**File**: `components/cdc_hal/src/WifiController.cpp`  
**Lines**: 547-598 (specifically line 565)

## Impact
- **User Experience**: When the system is under memory pressure, WiFi scans appear to return "no networks found" instead of showing previously cached results or indicating a transient failure.
- **Debuggability**: No log output when allocation fails, making it hard to diagnose memory issues.
- **Graceful Degradation**: The scan could return partial results (up to available buffer space) but instead returns nothing at all.

## Evidence
```cpp
// components/cdc_hal/src/WifiController.cpp:554-578
uint8_t WifiController::getScanResults(WifiScanResult* results, uint8_t maxResults) {
    if (!results || maxResults == 0 || !scanComplete_) {
        return 0;  // Could be OOM or empty - caller can't tell
    }

    uint16_t numAps = 0;
    esp_wifi_scan_get_ap_num(&numAps);

    if (numAps == 0) {
        return 0;
    }

    uint16_t toGet = (numAps < maxResults) ? numAps : maxResults;
    wifi_ap_record_t* apRecords = new (std::nothrow) wifi_ap_record_t[toGet];
    if (!apRecords) {
        LOG_E(TAG, "OOM allocating scan results buffer");  // Log exists but no return value distinction
        return 0;  // Same return as "no results" - caller can't differentiate
    }

    // ... rest of function
}
```

The function returns `0` for both:
1. No networks found (`numAps == 0`)
2. Memory allocation failure (`!apRecords`)

## Recommended Fix
Return a sentinel value or add an output parameter to distinguish OOM from empty results:

**Option 1: Return special value for OOM**
```cpp
uint8_t WifiController::getScanResults(WifiScanResult* results, uint8_t maxResults) {
    // ... existing checks ...
    
    wifi_ap_record_t* apRecords = new (std::nothrow) wifi_ap_record_t[toGet];
    if (!apRecords) {
        LOG_E(TAG, "OOM allocating scan results buffer");
        return 0xFF;  // Sentinel: 0-254 = count, 255 = OOM
    }
    // ...
}
```

**Option 2: Add status output parameter**
```cpp
uint8_t WifiController::getScanResults(WifiScanResult* results, uint8_t maxResults, 
                                       WifiScanStatus* status = nullptr) {
    // ...
    if (!apRecords) {
        if (status) *status = WifiScanStatus::OOM;
        return 0;
    }
    // ...
}
```

**Option 3: Return cached results on OOM** (most graceful)
```cpp
uint8_t WifiController::getScanResults(WifiScanResult* results, uint8_t maxResults) {
    // ...
    wifi_ap_record_t* apRecords = new (std::nothrow) wifi_ap_record_t[toGet];
    if (!apRecords) {
        LOG_W(TAG, "OOM allocating scan buffer, returning cached results");
        // Return cached scanResults_ instead of 0
        uint8_t count = (scanResultCount_ < maxResults) ? scanResultCount_ : maxResults;
        memcpy(results, scanResults_, count * sizeof(WifiScanResult));
        return count;
    }
    // ...
}
```

## References
- ESP32 WiFi scan API: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html#_CPPv422esp_wifi_scan_get_ap_numP10wifi_scan_t
- Graceful degradation pattern: Systems should continue operating at reduced capability when resources are constrained
