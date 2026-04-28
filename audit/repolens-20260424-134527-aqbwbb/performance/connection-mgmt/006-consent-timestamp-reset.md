---
title: "[MEDIUM] Consent timeout state not reset after accept"
severity: MEDIUM
domain: connection-mgmt
lens: performance/connection-mgmt
labels:
  - "audit:performance/connection-mgmt"
---

## Summary
In `components/mod_vcard/src/ble_vcard.cpp`, when consent is accepted via `ble_vcard_respond_consent(true)`, the `s_state_start_ms` timestamp is NOT reset. This means if the tick function is called again with the same `now_ms` value (or close), the elapsed time calculation might incorrectly trigger another timeout check.

**Location:** `components/mod_vcard/src/ble_vcard.cpp:1065-1081`

## Impact
1. **Erroneous timeout triggers**: If `ble_vcard_tick()` is called shortly after consent is accepted, the `consent_elapsed` calculation might still be based on the original consent start time. While `s_consent_pending` is cleared, any code that checks `s_state_start_ms` could see stale data.

2. **State pollution**: The `s_state_start_ms` variable is shared across multiple state machines (exchange states and consent). Not resetting it after consent completion pollutes the timestamp for subsequent operations.

3. **Debugging confusion**: When debugging timeout issues, stale `s_state_start_ms` values can mislead developers about when the actual state transition occurred.

## Evidence
**File: `components/mod_vcard/src/ble_vcard.cpp`**

1. **Consent timeout check (line 1135-1142):**
```cpp
// Server-side consent timeout
if (s_consent_pending) {
    uint32_t consent_elapsed = now_ms - s_state_start_ms;
    if (consent_elapsed > CONSENT_TIMEOUT_MS) {
        LOG_W(TAG, "Consent timeout, auto-declining");
        ble_vcard_respond_consent(false);
    }
}
```

2. **Consent start timestamp set (line 679):**
```cpp
s_consent_pending = true;
s_consent_conn_handle = connHandle;
s_state_start_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;  // Timestamp set
LOG_I(TAG, "Exchange request from connected device");
```

3. **Consent accepted but timestamp not reset (line 1065-1081):**
```cpp
void ble_vcard_respond_consent(bool accepted) {
    if (!s_consent_pending || s_consent_conn_handle == INVALID_HANDLE) return;

    if (accepted) {
        LOG_I(TAG, "Consent accepted, enabling receive");
        s_receive_enabled = true;
        sendStatusNotification(s_consent_conn_handle, STATUS_ACCEPTED);
        // s_state_start_ms NOT reset here!
    } else {
        LOG_I(TAG, "Consent declined");
        sendStatusNotification(s_consent_conn_handle, STATUS_DECLINED);
    }

    s_consent_pending = false;
    if (!accepted) {
        s_consent_conn_handle = INVALID_HANDLE;
    }
    // s_state_start_ms NOT reset here either!
}
```

4. **Other state transitions reset timestamp (line 389):**
```cpp
static void onConnect(uint16_t connHandle) {
    if (s_exchange_state == VCARD_EXCHANGE_CONNECTING) {
        s_client_conn_handle = connHandle;
        s_exchange_state = VCARD_EXCHANGE_DISCOVERING;
        s_state_start_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;  // Reset here
        // ...
    }
}
```

## Recommended Fix
Reset `s_state_start_ms` when consent is accepted or declined:

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
    s_state_start_ms = 0;  // Reset timestamp
    if (!accepted) {
        s_consent_conn_handle = INVALID_HANDLE;
    }
}
```

**Alternative approach**: Clear timestamp when consent is no longer pending (at the end of the function):
```cpp
s_consent_pending = false;
s_state_start_ms = 0;  // Clear regardless of accept/decline
if (!accepted) {
    s_consent_conn_handle = INVALID_HANDLE;
}
```

## References
- [FreeRTOS Tick Timer](https://www.freertos.org/a00005.html) - Understanding tick-based timing
- [State Machine Timing](https://en.wikipedia.org/wiki/Finite-state_machine#Time-dependent_state_machines) - Proper timestamp management in state machines
