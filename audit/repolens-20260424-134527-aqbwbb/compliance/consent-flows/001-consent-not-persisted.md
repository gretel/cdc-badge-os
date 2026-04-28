---
title: "[MEDIUM] Consent state not persisted across power cycles"
severity: MEDIUM
domain: compliance
lens: consent-flows
labels:
  - consent-persistence
---

## Summary

In the vCard BLE exchange module (`components/mod_vcard/src/ble_vcard.cpp`), consent state is stored only in volatile memory (`static bool s_consent_pending`). When a peer requests vCard exchange, the user is prompted to accept or decline, but this consent decision is **not persisted** to NVS (Non-Volatile Storage).

**Location:** `components/mod_vcard/src/ble_vcard.cpp:166-168`
```cpp
static bool s_consent_pending = false;
static char s_consent_peer_name[VCARD_BLE_NAME_MAX] = {};
static uint16_t s_consent_conn_handle = INVALID_HANDLE;
```

**Location:** `components/mod_vcard/src/ble_vcard.cpp:1067-1079`
```cpp
void ble_vcard_respond_consent(bool accepted) {
    if (!s_consent_pending || s_consent_conn_handle == INVALID_HANDLE) return;

    if (accepted) {
        LOG_I(TAG, "Consent accepted, enabling receive");
        s_receive_enabled = true;
        sendStatusNotification(s_consent_conn_handle, STATUS_ACCEPTED);
    } else {
        LOG_I(TAG, "Consent declined");
        ...
    }

    s_consent_pending = false;  // Only volatile state updated
    ...
}
```

## Impact

1. **No audit trail:** When consent is given, there is no permanent record of:
   - Which peer was accepted
   - When the consent was given
   - What the user's consent preference was

2. **Lost context on reboot:** If the device reboots after a consent decision, the system has no way to recall which peers were previously accepted or declined.

3. **Difficulty in compliance:** For GDPR-style "right to know" requests, there is no stored record of consent history that could be retrieved.

## Evidence

The consent state uses only static variables:
- `s_consent_pending` (line 166) - volatile boolean
- `s_consent_peer_name` (line 167) - volatile string
- `s_consent_conn_handle` (line 168) - volatile connection handle

No NVS storage is used for consent tracking. Compare to vCard storage which properly uses NVS:
```cpp
// vcard_store.cpp:346-357 - Proper NVS persistence for vCard data
nvs_handle_t nvs;
if (nvs_open(VCARD_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
    ...
}
esp_err_t ret = nvs_set_str(nvs, VCARD_KEY_OWN, tmp);
ret = nvs_commit(nvs);
```

## Recommended Fix

Add NVS-based persistence for consent decisions:

1. Create a consent record structure with:
   - Peer identifier (address or name hash)
   - Timestamp of consent
   - Consent decision (accepted/declined)
   - Policy version (for future tracking)

2. Store consent records in NVS with keys like `consent_<peer_hash>`

3. Load consent history on module initialization

4. Provide API to query consent history for a given peer

Basic implementation sketch:
```cpp
typedef struct {
    uint64_t peer_addr;
    uint32_t timestamp_ms;
    bool accepted;
    uint8_t policy_version;
} consent_record_t;

static void save_consent_record(const uint8_t* addr, bool accepted) {
    nvs_handle_t nvs;
    nvs_open("consent", NVS_READWRITE, &nvs);
    // Store record with peer address as key
    nvs_commit(nvs);
    nvs_close(nvs);
}
```

## References

- GDPR Article 7(1): "The controller shall be able to demonstrate that the data subject is prepared to consent"
- GDPR Article 7(3): "It shall be as easy to withdraw as to give consent"
- ISO/IEC 29100: Privacy framework for consent record-keeping
