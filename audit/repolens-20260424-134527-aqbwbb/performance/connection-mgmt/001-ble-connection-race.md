---
title: "[MEDIUM] Race condition in BLE vCard connection state management"
severity: MEDIUM
domain: connection-mgmt
lens: performance/connection-mgmt
labels:
  - "audit:performance/connection-mgmt"
---

## Summary
In `components/mod_vcard/src/ble_vcard.cpp`, the BLE vCard exchange module uses two static connection handle variables (`s_client_conn_handle` and `s_consent_conn_handle`) to track active connections. These are updated in callback functions (`onConnect`, `onDisconnect`, GATT write callbacks) that can be invoked from the NimBLE host task context. However, there is no mutex protection around these connection handle updates and reads, creating potential race conditions.

**Specific locations:**
- `s_client_conn_handle` (line 151) - updated in `onConnect()` (line 387), read in multiple places
- `s_consent_conn_handle` (line 169) - updated in GATT write callback (line 678), read in `onDisconnect()` (line 414)
- `onDisconnect()` at line 404-420 checks and clears connection handles without synchronization

## Impact
Race conditions in connection handle management can lead to:
1. **Use-after-close bugs**: A connection might be used after it's been disconnected if the disconnect callback runs before the main logic checks the handle
2. **Connection leaks**: If `onDisconnect()` clears `s_client_conn_handle` but another thread is in the middle of an exchange flow, the connection might not be properly terminated
3. **Duplicate disconnections**: Multiple code paths might try to disconnect the same handle if state checks are not atomic
4. **Inconsistent exchange state**: The exchange state machine (`s_exchange_state`) and connection handles are not synchronized, potentially leading to operations on stale connections

This is particularly critical because:
- The module supports both client-role (outbound exchange) and server-role (inbound consent) simultaneously
- Connection callbacks are invoked from the BLE host task (different context)
- The `ble_vcard_tick()` function (line 1099) also accesses connection state for timeout handling

## Evidence
**File: `components/mod_vcard/src/ble_vcard.cpp`**

1. **Unprotected connection handle update in onConnect (line 385-394):**
```cpp
static void onConnect(uint16_t connHandle) {
    if (s_exchange_state == VCARD_EXCHANGE_CONNECTING) {
        s_client_conn_handle = connHandle;  // No mutex protection
        s_exchange_state = VCARD_EXCHANGE_DISCOVERING;  // State update not atomic with handle
        // ...
    }
}
```

2. **Unprotected connection handle check in onDisconnect (line 404-420):**
```cpp
static void onDisconnect(uint16_t connHandle, int reason) {
    if (connHandle == s_client_conn_handle) {  // Read without lock
        s_client_conn_handle = INVALID_HANDLE;  // Write without lock
        // ...
    } else {
        if (connHandle == s_consent_conn_handle) {  // Another unprotected read
            s_consent_conn_handle = INVALID_HANDLE;
            // ...
        }
    }
}
```

3. **GATT write callback sets connection handle (line 664-680):**
```cpp
s_gattChars[2].onWrite = [](uint16_t connHandle, uint16_t, const uint8_t* data, uint16_t len) -> int {
    // ...
    s_consent_conn_handle = connHandle;  // Unprotected write from GATT context
    // ...
}
```

4. **Multiple unprotected reads in exchange functions:**
- `sendExchangeRequest()` (line 470) reads `s_client_conn_handle`
- `readPeerVcard()` (line 494) reads `s_client_conn_handle`
- `writePeerVcard()` (line 509) reads `s_client_conn_handle`

5. **Tick function accesses connection state (line 1099-1145):**
```cpp
void ble_vcard_tick(uint32_t now_ms) {
    // ...
    if (s_consent_pending) {  // Reads state set in GATT callback
        uint32_t consent_elapsed = now_ms - s_state_start_ms;
        if (consent_elapsed > CONSENT_TIMEOUT_MS) {
            ble_vcard_respond_consent(false);  // Might access s_consent_conn_handle
        }
    }
}
```

## Recommended Fix
Add mutex protection around all connection handle accesses. The module already has a mutex for peer list (`s_peer_mutex`), but needs a separate mutex for connection state:

1. **Add a connection state mutex:**
```cpp
static SemaphoreHandle_t s_conn_mutex = nullptr;  // Add near line 115
```

2. **Initialize in `ble_vcard_init()` (after line 730):**
```cpp
if (!s_conn_mutex) {
    s_conn_mutex = xSemaphoreCreateMutex();
    if (!s_conn_mutex) {
        LOG_E(TAG, "Failed to create conn mutex");
        return false;
    }
}
```

3. **Protect connection handle updates in onConnect:**
```cpp
static void onConnect(uint16_t connHandle) {
    if (xSemaphoreTake(s_conn_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (s_exchange_state == VCARD_EXCHANGE_CONNECTING) {
            s_client_conn_handle = connHandle;
            s_exchange_state = VCARD_EXCHANGE_DISCOVERING;
            s_state_start_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        }
        xSemaphoreGive(s_conn_mutex);
    }
    // ... rest of discovery logic outside lock
}
```

4. **Protect connection handle reads/writes in onDisconnect:**
```cpp
static void onDisconnect(uint16_t connHandle, int reason) {
    if (xSemaphoreTake(s_conn_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (connHandle == s_client_conn_handle) {
            s_client_conn_handle = INVALID_HANDLE;
            // ...
        } else if (connHandle == s_consent_conn_handle) {
            s_consent_conn_handle = INVALID_HANDLE;
            // ...
        }
        xSemaphoreGive(s_conn_mutex);
    }
}
```

5. **Protect GATT callback:**
```cpp
s_gattChars[2].onWrite = [](uint16_t connHandle, uint16_t, const uint8_t* data, uint16_t len) -> int {
    if (len < 1) return 0;
    
    if (xSemaphoreTake(s_conn_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (data[0] == CMD_REQUEST_EXCHANGE) {
            if (s_consent_pending || s_exchange_in_progress) {
                xSemaphoreGive(s_conn_mutex);
                sendStatusNotification(connHandle, STATUS_BUSY);
                return 0;
            }
            s_consent_pending = true;
            s_consent_conn_handle = connHandle;
            s_state_start_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        } else if (data[0] == CMD_CANCEL) {
            if (s_consent_pending && s_consent_conn_handle == connHandle) {
                s_consent_pending = false;
                s_consent_conn_handle = INVALID_HANDLE;
            }
        }
        xSemaphoreGive(s_conn_mutex);
    }
    return 0;
};
```

6. **Protect tick function access:**
```cpp
void ble_vcard_tick(uint32_t now_ms) {
    // ... scan processing ...
    
    bool consent_pending = false;
    uint32_t consent_start_ms = 0;
    
    if (xSemaphoreTake(s_conn_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        consent_pending = s_consent_pending;
        consent_start_ms = s_state_start_ms;
        xSemaphoreGive(s_conn_mutex);
    }
    
    if (consent_pending) {
        uint32_t consent_elapsed = now_ms - consent_start_ms;
        if (consent_elapsed > CONSENT_TIMEOUT_MS) {
            ble_vcard_respond_consent(false);
        }
    }
}
```

## References
- [FreeRTOS Semaphore API](https://www.freertos.org/a00102.html) - For mutex usage patterns
- [BLE Connection Lifecycle](https://docs.nimble-mcu.org/group__group-nimble-group-gap/) - NimBLE connection event timing
- [ESP32 BLE Architecture](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/bluetooth/index.html) - Callback execution context
