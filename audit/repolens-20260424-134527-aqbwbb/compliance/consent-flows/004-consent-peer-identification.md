---
title: "[HIGH] Peer identification too generic for meaningful consent"
severity: HIGH
domain: compliance
lens: consent-flows
labels:
  - peer-identification
  - consent-clarity
---

## Summary

When a consent request is triggered in the vCard module, the peer name used for identification is hardcoded to `"BLE Device"` instead of using the actual peer name discovered during scanning. This makes consent decisions essentially meaningless as the user cannot identify who is requesting their vCard.

**Location:** `components/mod_vcard/src/ble_vcard.cpp:672-675`

## Impact

1. **Informed consent impossible:** GDPR requires consent to be "informed" - users need to know *who* they're giving consent to. With all peers appearing as "BLE Device", users cannot make informed decisions.

2. **No differentiation:** Multiple peers requesting exchange all appear identical, making it impossible to selectively accept/decline based on peer identity.

3. **Audit trail useless:** Even if consent is recorded, the record shows "BLE Device" for all entries, making historical records meaningless.

4. **User experience degraded:** Users see generic "BLE Device" instead of actual peer names, reducing trust in the system.

## Evidence

**ble_vcard.cpp:660-685** - GATT control characteristic write handler:
```cpp
s_gattChars[2].onWrite = [](uint16_t connHandle, uint16_t, const uint8_t* data, uint16_t len) -> int {
    if (len < 1) return 0;

    if (data[0] == CMD_REQUEST_EXCHANGE) {
        if (s_consent_pending || s_exchange_in_progress) {
            sendStatusNotification(connHandle, STATUS_BUSY);
            return 0;
        }

        // Peer name from scan list is not easily available here;
        // connHandle-to-address mapping would need API extension
        snprintf(s_consent_peer_name, sizeof(s_consent_peer_name), "BLE Device");  // <-- Hardcoded!

        s_consent_pending = true;
        s_consent_conn_handle = connHandle;
        s_state_start_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        LOG_I(TAG, "Exchange request from connected device");

        if (s_consent_callback) {
            s_consent_callback(s_consent_peer_name);  // Passes generic name
        }
    }
    ...
};
```

**VcardModule.cpp:154-166** - Consent prompt uses this generic name:
```cpp
static void onConsentRequest(const char* peerName) {
    static char promptText[256];
    snprintf(promptText, sizeof(promptText),
             "%s\n\n%s\nmoechte vCard tauschen\n\n[Y] %s\n[N] %s",
             mstr(STR_EXCHANGE_REQ),
             peerName,  // Will show "BLE Device"
             mstr(STR_ACCEPT),
             mstr(STR_DECLINE));
    ...
}
```

The code even has a comment acknowledging the limitation:
```cpp
// Peer name from scan list is not easily available here;
// connHandle-to-address mapping would need API extension
```

## Recommended Fix

1. **Map connection handle to peer address:**
```cpp
// Add to IBluetoothController interface:
uint8_t getPeerAddress(uint16_t connHandle, uint8_t* addr, uint8_t* addr_type);
```

2. **Look up peer name from scan cache:**
```cpp
static void lookup_peer_name(uint16_t connHandle, char* out_name, size_t max_len) {
    auto* ble = getBle();
    uint8_t addr[6];
    uint8_t addr_type;
    
    if (ble->getPeerAddress(connHandle, addr, &addr_type)) {
        // Search scan cache for matching peer
        if (xSemaphoreTake(s_peer_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            for (uint16_t i = 0; i < s_peer_count; i++) {
                if (memcmp(s_peers[i].addr, addr, 6) == 0) {
                    strncpy(out_name, s_peers[i].name, max_len - 1);
                    break;
                }
            }
            xSemaphoreGive(s_peer_mutex);
        }
    }
    
    if (out_name[0] == '\0') {
        // Fallback: show address
        snprintf(out_name, max_len, "%02X:%02X:%02X:%02X:%02X:%02X",
                 addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    }
}
```

3. **Call lookup in GATT handler:**
```cpp
if (data[0] == CMD_REQUEST_EXCHANGE) {
    lookup_peer_name(connHandle, s_consent_peer_name, sizeof(s_consent_peer_name));
    s_consent_pending = true;
    s_consent_conn_handle = connHandle;
    ...
}
```

## References

- GDPR Article 4(11): "Consent... shall be specific"
- GDPR Article 7(2): "The controller shall... clearly distinguish consent from other matters"
- ePrivacy Directive: Clear identification of data recipients
