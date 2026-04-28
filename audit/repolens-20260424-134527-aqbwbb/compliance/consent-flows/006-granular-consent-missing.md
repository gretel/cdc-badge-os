---
title: "[MEDIUM] Receive mode enabled globally without per-peer consent tracking"
severity: MEDIUM
domain: compliance
lens: consent-flows
labels:
  - granular-consent
  - per-peer-tracking
---

## Summary

In the vCard BLE exchange module (`components/mod_vcard/src/ble_vcard.cpp`), when a user accepts consent for an incoming exchange request, the system sets a **global flag** `s_receive_enabled = true` instead of tracking consent on a per-peer basis. This means:

1. Once any peer is accepted, ALL peers can write to the RX characteristic
2. There is no way to restrict data reception to only previously accepted peers
3. The consent decision is overly broad and not granular enough

**Location:** `components/mod_vcard/src/ble_vcard.cpp:636` and `1070`

## Impact

1. **Non-granular consent:** GDPR requires consent to be "specific" - meaning it should be tied to a particular data subject (peer). Currently, consent for one peer opens the door for all peers.

2. **No selective control:** Users cannot accept one peer but decline another. The consent is all-or-nothing for all peers.

3. **Data exposure risk:** Any nearby device with the vCard service can write data once one peer has been accepted, not just the specific peer that was consented to.

4. **Inconsistent with user intent:** When a user sees "BLE Device wants to exchange vCards" and clicks "Accept", they likely mean "accept THIS device", not "accept all devices".

## Evidence

**ble_vcard.cpp:97** - Global receive flag:
```cpp
static bool s_receive_enabled = false;
```

**ble_vcard.cpp:636** - RX characteristic write permission check (only checks global flag):
```cpp
s_gattChars[1].onWrite = [](uint16_t, uint16_t, const uint8_t* data, uint16_t len) -> int {
    if (!s_receive_enabled) return 0x03; // Write not permitted
    // ... process vCard data
};
```

**ble_vcard.cpp:1067-1079** - Consent response enables global receive:
```cpp
void ble_vcard_respond_consent(bool accepted) {
    if (!s_consent_pending || s_consent_conn_handle == INVALID_HANDLE) return;

    if (accepted) {
        LOG_I(TAG, "Consent accepted, enabling receive");
        s_receive_enabled = true;  // <-- GLOBAL FLAG, not per-peer!
        sendStatusNotification(s_consent_conn_handle, STATUS_ACCEPTED);
    } else {
        LOG_I(TAG, "Consent declined");
        sendStatusNotification(s_consent_conn_handle, STATUS_DECLINED);
    }

    s_consent_pending = false;
    if (!accepted) {
        s_consent_conn_handle = INVALID_HANDLE;
    }
}
```

Note that `s_consent_conn_handle` tracks which connection the consent was for, but this information is lost after the consent is processed - it's not used to restrict future access to only that peer.

## Recommended Fix

Implement per-peer consent tracking:

1. **Define a consent record structure:**
```cpp
typedef struct {
    uint8_t addr[6];
    uint8_t addr_type;
    bool accepted;
    uint32_t timestamp_ms;
} peer_consent_t;

static constexpr uint8_t MAX_CONSENTED_PEERS = 8;
static peer_consent_t s_consented_peers[MAX_CONSENTED_PEERS] = {};
static uint8_t s_consented_peer_count = 0;
```

2. **Update consent response to record per-peer consent:**
```cpp
void ble_vcard_respond_consent(bool accepted) {
    if (!s_consent_pending || s_consent_conn_handle == INVALID_HANDLE) return;

    // Get peer address from connection handle
    uint8_t peer_addr[6];
    uint8_t peer_addr_type;
    auto* ble = getBle();
    ble->getPeerAddress(s_consent_conn_handle, peer_addr, &peer_addr_type);

    if (accepted) {
        // Record consent for this specific peer
        save_peer_consent(peer_addr, peer_addr_type, true);
        sendStatusNotification(s_consent_conn_handle, STATUS_ACCEPTED);
    } else {
        // Record consent for this specific peer
        save_peer_consent(peer_addr, peer_addr_type, false);
        sendStatusNotification(s_consent_conn_handle, STATUS_DECLINED);
    }

    s_consent_pending = false;
    s_consent_conn_handle = INVALID_HANDLE;
}
```

3. **Update RX characteristic to check per-peer consent:**
```cpp
s_gattChars[1].onWrite = [](uint16_t connHandle, uint16_t, const uint8_t* data, uint16_t len) -> int {
    // Get peer address from connection handle
    uint8_t peer_addr[6];
    uint8_t peer_addr_type;
    auto* ble = getBle();
    ble->getPeerAddress(connHandle, peer_addr, &peer_addr_type);
    
    // Check if this specific peer has consented
    if (!has_peer_consent(peer_addr, peer_addr_type)) {
        return 0x03; // Write not permitted
    }
    
    // ... process vCard data
};
```

4. **Add API to manage per-peer consent:**
```cpp
bool ble_vcard_has_peer_consent(const uint8_t* addr, uint8_t addr_type);
void ble_vcard_revoke_peer_consent(const uint8_t* addr, uint8_t addr_type);
uint8_t ble_vcard_get_consented_peers(peer_consent_t* out, uint8_t max_peers);
```

## References

- GDPR Article 4(11): "Consent... shall be specific"
- GDPR Article 7(2): "The controller shall... clearly distinguish consent from other matters"
- ISO/IEC 29100: Granular consent requirements for different data subjects

</content>