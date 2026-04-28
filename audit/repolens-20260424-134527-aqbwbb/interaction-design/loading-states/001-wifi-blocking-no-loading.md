---
title: "[MEDIUM] WiFi connect/NTP sync blocks UI without loading indicator"
severity: MEDIUM
domain: interaction-design/loading-states
lens: loading-states
labels:
  - "audit:interaction-design/loading-states"
---

## Summary
When WiFi connection or NTP synchronization is triggered (e.g., via `WifiHandlers::connect()` or `WifiHandlers::syncNtp()` in `components/cdc_os_ui/src/WifiHandlers.cpp:134-273`), the operations execute **synchronously with blocking delays** but provide **no visual feedback** to the user. The UI appears frozen with no indication that work is in progress.

**Evidence:**
- `WifiHandlers::connect()` (line 134-174): Calls `wifi->connect()` with 15-second timeout, blocking the calling thread
- `WifiHandlers::syncNtp()` (line 192-273): Contains blocking loop with `vTaskDelay(pdMS_TO_TICKS(100))` waiting for NTP sync (up to 10 seconds)
- No loading state, spinner, toast, or progress indicator is shown during these operations

## Impact
**User Experience:** Users see a frozen screen with no feedback during WiFi connection (up to 15s) or NTP sync (up to 10s). They may think the device is frozen and press keys repeatedly, causing confusion.

**Technical:** The blocking calls are acceptable for embedded context, but the lack of UI feedback violates basic interaction design principles for async operations.

## Evidence
**File: `components/cdc_os_ui/src/WifiHandlers.cpp`**

```cpp
// Line 134-174: Blocking WiFi connect with no UI feedback
bool WifiHandlers::connect() {
    // ... setup ...
    bool connected = wifi->connect(config_.ssid, config_.password, WIFI_CONNECT_TIMEOUT_MS);
    // 15-second blocking wait here with no loading indicator
    if (!connected || !wifi->isConnected()) {
        // ... error handling ...
    }
    return true;
}

// Line 228-245: Blocking NTP sync loop
while ((esp_timer_get_time() / 1000 - startMs) < NTP_SYNC_TIMEOUT_MS) {
    if (esp_sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
        synced = true;
        break;
    }
    vTaskDelay(pdMS_TO_TICKS(100));  // Blocking 100ms loops, no UI update
}
```

**File: `components/cdc_os_ui/src/AppUi.cpp`** (where these are called)
- No loading state is set before calling `connect()` or `syncNtp()`
- No toast or indicator is shown to inform the user

## Recommended Fix
Add a loading indicator before triggering blocking operations:

1. **Before WiFi connect/NTP sync:** Show a toast with `showToastTask("Connecting...")` or similar
2. **After completion/error:** Dismiss the toast with `ViewStack::instance().hideModal()`
3. **Example pattern:**
```cpp
void WifiHandlers::connectWithUi() {
    showToastTask("Connecting to WiFi...");
    ViewStack::instance().render();  // Force render the toast
    
    bool connected = connect();
    
    ViewStack::instance().hideModal();  // Dismiss loading
    if (connected) {
        showToastSuccess("Connected");
    } else {
        showToastError(getLastError());
    }
}
```

## References
- Loading State Handling best practices: Users need immediate feedback during operations > 100ms
- ToastView already has `Icon::TASK` for progress indicators (`components/cdc_views/include/cdc_views/ToastView.h:24`)
