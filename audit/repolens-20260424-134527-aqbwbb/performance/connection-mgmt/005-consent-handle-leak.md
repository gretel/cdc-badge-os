---
title: "[HIGH] BLE vCard consent connection handle not cleared on accept"
severity: HIGH
domain: connection-mgmt
lens: performance/connection-mgmt
labels:
  - "audit:performance/connection-mgmt"
---

## Summary
In `components/mod_vcard/src/ble_vcard.cpp`, when a consent request is accepted via `ble_vcard_respond_consent(true)`, the `s_consent_conn_handle` is NOT cleared. This causes the connection handle to linger indefinitely, potentially leading to:
1. Stale connection tracking
2. Confusion in the consent state machine
3. Potential duplicate notifications to the same peer

**Location:** `components/mod_vcard/src/ble_vcard.cpp:1065-1081`

## Impact
1. **State inconsistency**: `s_consent_pending` is cleared (line 1077), but `s_consent_conn_handle` retains the old value. This creates a "dangling" handle that no longer represents an active consent state.

2. **Confusion in subsequent operations**: Code that checks `s_consent_conn_handle != INVALID_HANDLE` might incorrectly assume there's an active consent, even though `s_consent_pending` is false.

3. **Memory leak (logical)**: While not a heap memory leak, the connection handle state is not properly reset, which could cause issues if the same peer reconnects or if multiple consent cycles occur.

4. **Debugging difficulty**: The asymmetry (clearing `s_consent_pending` but not `s_consent_conn_handle` on accept) makes the code harder to understand and maintain.

## Evidence
**File: `components/mod_vcard/src/ble_vcard.cpp`**

1. **Current implementation (line 1065-1081):**
```cpp
void ble_vcard_respond_consent(bool accepted) {
    if (!s_consent_pending || s_consent_conn_handle == INVALID_HANDLE) return;

    if (accepted) {
        LOG_I(TAG, "Consent accepted, enabling receive");
        s_receive_enabled = true;
        sendStatusNotification(s_consent_conn_handle, STATUS_ACCEPTED);
    } else {
        LOG_I(TAG, "Consent declined");
        sendStatusNotification(s_consent_conn_handle, STATUS_DECLINED);
    }

    s_consent_pending = false;
    if (!accepted) {
        s_consent_conn_handle = INVALID_HANDLE;  // Only cleared on decline!
    }
}
```

2. **Consistent pattern in onDisconnect (line 414-417):**
```cpp
static void onDisconnect(uint16_t connHandle, int reason) {
    // ...
    if (connHandle == s_consent_conn_handle) {
        s_consent_pending = false;
        s_consent_conn_handle = INVALID_HANDLE;  // Both cleared together
    }
    // ...
}
```

3. **Consistent pattern in onWrite callback (line 686-688):**
```cpp
} else if (data[0] == CMD_CANCEL) {
    if (s_consent_pending && s_consent_conn_handle == connHandle) {
        s_consent_pending = false;
        s_consent_conn_handle = INVALID_HANDLE;  // Both cleared together
    }
}
```

4. **Variable declarations (line 167-169):**
```cpp
static bool s_consent_pending = false;
static uint16_t s_consent_conn_handle = INVALID_HANDLE;
```

The pattern across the codebase shows that `s_consent_pending` and `s_consent_conn_handle` should be cleared together, but `ble_vcard_respond_consent()` breaks this pattern on the "accept" path.

## Recommended Fix
Clear `s_consent_conn_handle` when accepting consent, consistent with the pattern used elsewhere:

```cpp
void ble_vcard_respond_consent(bool accepted) {
    if (!s_consent_pending || s_consent_conn_handle == INVALID_HANDLE) return;

    if (accepted) {
        LOG_I(TAG, "Consent accepted, enabling receive");
        s_receive_enabled = true;
        sendStatusNotification(s_consent_conn_handle, STATUS_ACCEPTED);
        s_consent_conn_handle = INVALID_HANDLE;  // Add this line
    } else {
        LOG_I(TAG, "Consent declined");
        sendStatusNotification(s_consent_conn_handle, STATUS_DECLINED);
    }

    s_consent_pending = false;
}
```

**Alternatively**, if the connection handle needs to be preserved for later use after consent is accepted (e.g., for ongoing data transfer), the code should:
1. Transfer the handle to a different variable (e.g., `s_active_exchange_handle`)
2. Clear `s_consent_conn_handle` to indicate consent phase is complete
3. Document this state transition clearly

## References
- [NimBLE Connection Lifecycle](https://docs.nimble-mcu.org/group__group-nimble-group-gap/) - BLE connection state management
- [ESP32 BLE Best Practices](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/bluetooth/ble_gap.html) - Connection handle management
- [State Machine Consistency](https://en.wikipedia.org/wiki/Finite-state_machine#Design) - Ensuring state variables are kept consistent
