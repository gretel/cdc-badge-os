---
title: "[LOW] Missing loading state during vCard BLE exchange"
severity: LOW
domain: interaction-design/loading-states
lens: loading-states
labels:
  - "audit:interaction-design/loading-states"
---

## Summary
When initiating a vCard BLE exchange, the UI shows a toast but doesn't provide continuous feedback during the connection and transfer process. The `onPeerSelect()` function in `components/mod_vcard/src/VcardModule.cpp:286-297` calls `ble_vcard_exchange_with()` which involves BLE connection, data transfer, and callback handling.

**Location**: `components/mod_vcard/src/VcardModule.cpp:286-297`

## Impact
- BLE operations can take 2-5 seconds depending on connection quality
- Only initial "Connecting..." toast is shown, no progress feedback
- User might think the operation failed and try again
- The `STR_CONNECTING` toast is shown but never explicitly dismissed

## Evidence
```cpp
// components/mod_vcard/src/VcardModule.cpp:286-297
static void onPeerSelect(uint16_t index, void* userData) {
    (void)userData;

    if (index >= s_uiPeerCount) return;

    vcard_peer_t& peer = s_uiPeers[index];

    // Start exchange
    if (ble_vcard_exchange_with(peer.addr, peer.addr_type)) {
        ui::showToastInfo(mstr(STR_CONNECTING));  // Shown but no progress
    } else {
        ui::showToastError("Exchange failed");
    }
}
```

The exchange complete callback is set up in `init()` at line 466:
```cpp
ble_vcard_set_exchange_complete_callback(onExchangeComplete);
```

But there's a gap between showing "Connecting..." and the final success/failure toast.

## Recommended Fix
Use a task toast that persists until the exchange completes:

```cpp
static void onPeerSelect(uint16_t index, void* userData) {
    (void)userData;

    if (index >= s_uiPeerCount) return;

    vcard_peer_t& peer = s_uiPeers[index];

    // Start exchange with persistent loading indicator
    ui::showToastTask(mstr(STR_CONNECTING));  // Use task toast (doesn't auto-dismiss)
    
    if (!ble_vcard_exchange_with(peer.addr, peer.addr_type)) {
        ui::showToastError("Exchange failed");
    }
}

static void onExchangeComplete(bool success, const char* error) {
    if (success) {
        ui::showToastSuccess(mstr(STR_EXCHANGE_OK));
    } else {
        if (error && error[0]) {
            ui::showToastError(error);
        } else {
            ui::showToastError(mstr(STR_EXCHANGE_FAIL));
        }
    }
}
```

Alternatively, use `showToastTask()` which defaults to non-dismissible, and the success/error toast will replace it automatically.

## References
- BLE operations typically take 2-5 seconds for connection and data transfer
- ToastView already supports task indicator: `showToastTask()` in `components/cdc_views/src/ToastView.cpp`
