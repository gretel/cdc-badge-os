---
title: "[MEDIUM] High cyclomatic complexity in bleGapEventCallback function"
severity: MEDIUM
domain: code-quality
lens: cyclomatic-complexity
labels:
  - "complexity:high"
---

## Summary
The `bleGapEventCallback` function in `components/cdc_hal/src/BluetoothController.cpp:352` has high cyclomatic complexity with 10 switch cases and multiple nested conditionals. The function handles BLE GAP events and has approximately 12-14 independent code paths.

**Evidence:**
- File: `components/cdc_hal/src/BluetoothController.cpp`
- Lines: 352-419 (67 lines total)
- Switch cases: 10 (BLE_GAP_EVENT_CONNECT, DISCONNECT, CONN_UPDATE, ADV_COMPLETE, MTU, DISC, DISC_COMPLETE, PASSKEY_ACTION, SUBSCRIBE, NOTIFY_RX)
- Nested conditionals: 3 (CONNECT status check, NOTIFY_RX with callback check, etc.)

## Impact
- **Maintainability**: Adding new BLE events requires understanding all existing branches
- **Testing**: Each code path needs separate test coverage
- **Bug risk**: Modifying one case could inadvertently affect others due to shared state
- **Readability**: Developers must scroll and track multiple branches to understand flow

## Evidence
```cpp
int bleGapEventCallback(struct ble_gap_event* event, void* arg) {
    (void)arg;
    auto* ctrl = BluetoothController::instance_;
    if (!ctrl) return 0;

    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:       // Case 1 + nested if/else
            if (event->connect.status == 0) { ... } else { ... }
            break;
        case BLE_GAP_EVENT_DISCONNECT:    // Case 2
            ctrl->onDisconnect(...);
            break;
        case BLE_GAP_EVENT_CONN_UPDATE:   // Case 3
            LOG_I(TAG, "Connection updated");
            break;
        // ... 7 more cases
        case BLE_GAP_EVENT_NOTIFY_RX:     // Case 10 with nested if
            if (ctrl->notifyCb_ && event->notify_rx.om) { ... }
            break;
        default:
            break;
    }
    return 0;
}
```

## Recommended Fix
Split the callback dispatcher into smaller, focused handler functions:

1. Create individual handler methods for each event type:
   ```cpp
   void BluetoothController::handleConnect(struct ble_gap_event* event);
   void BluetoothController::handleDisconnect(struct ble_gap_event* event);
   void BluetoothController::handleScanResult(struct ble_gap_event* event);
   // etc.
   ```

2. Refactor `bleGapEventCallback` to delegate:
   ```cpp
   int bleGapEventCallback(struct ble_gap_event* event, void* arg) {
       auto* ctrl = BluetoothController::instance_;
       if (!ctrl) return 0;
       
       switch (event->type) {
           case BLE_GAP_EVENT_CONNECT:
               ctrl->handleConnect(event);
               break;
           // ... delegate to handlers
       }
       return 0;
   }
   ```

3. Each handler should be 10-15 lines max with single responsibility.

**Estimated effort**: 45-60 minutes

## References
- Cyclomatic Complexity: https://en.wikipedia.org/wiki/Cyclomatic_complexity
- Martin, R. C. (2008). Clean Code. Chapter 6: Single Responsibility Principle
- ESP-NIMBLE documentation: https://github.com/apache/mynewt-mcumgr/blob/main/nimble/transport/README.md
