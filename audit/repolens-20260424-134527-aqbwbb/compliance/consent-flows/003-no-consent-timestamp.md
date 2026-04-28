---
title: "[MEDIUM] Consent recorded without timestamp for audit trail"
severity: MEDIUM
domain: compliance
lens: consent-flows
labels:
  - consent-timestamp
  - audit-trail
---

## Summary

When consent is given for vCard exchange in `components/mod_vcard/src/ble_vcard.cpp`, the decision is recorded without any timestamp. This makes it impossible to:
- Determine when consent was given
- Track consent changes over time
- Provide audit trail for compliance purposes

**Location:** `components/mod_vcard/src/ble_vcard.cpp:1067-1079`

## Impact

1. **No temporal context:** Without timestamps, consent records lack crucial context about when decisions were made.

2. **Policy versioning impossible:** If consent policies change, there's no way to determine which policy version was in effect when consent was given.

3. **Compliance gap:** GDPR Article 7(1) requires controllers to "demonstrate that the data subject is prepared to consent" - timestamps are essential for this proof.

4. **Troubleshooting difficulty:** When investigating exchange issues, there's no way to correlate timing of consent decisions with other events.

## Evidence

**ble_vcard.cpp:166-170** - Consent state variables (no timestamp):
```cpp
static bool s_consent_pending = false;
static char s_consent_peer_name[VCARD_BLE_NAME_MAX] = {};
static uint16_t s_consent_conn_handle = INVALID_HANDLE;
```

**ble_vcard.cpp:1067-1079** - Consent response (no timestamp recorded):
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
        s_consent_conn_handle = INVALID_HANDLE;
    }
}
```

Note that `s_state_start_ms` is used for timeout tracking (line 675) but the actual consent decision timestamp is never recorded.

## Recommended Fix

Add timestamp recording when consent is given:

1. **Define consent record structure:**
```cpp
typedef struct {
    uint32_t timestamp_ms;      // Uptime when consent given
    uint64_t peer_addr;         // Peer BLE address
    bool accepted;              // Decision
    uint8_t policy_version;     // Policy version at time of consent
} consent_record_t;
```

2. **Record timestamp when consent is responded:**
```cpp
void ble_vcard_respond_consent(bool accepted) {
    if (!s_consent_pending || s_consent_conn_handle == INVALID_HANDLE) return;

    uint32_t consent_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

    if (accepted) {
        save_consent_record(s_consent_conn_handle, consent_time, true);
        sendStatusNotification(s_consent_conn_handle, STATUS_ACCEPTED);
    } else {
        save_consent_record(s_consent_conn_handle, consent_time, false);
        sendStatusNotification(s_consent_conn_handle, STATUS_DECLINED);
    }
    ...
}
```

3. **Store in NVS** with timestamp for persistence across reboots

## References

- GDPR Article 7(1): "The controller shall be able to demonstrate that the data subject is prepared to consent"
- GDPR Article 5(2) "Accountability": Need to demonstrate compliance with consent requirements
- ISO/IEC 29100: Timestamp requirements for consent records
