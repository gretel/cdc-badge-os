---
title: "[LOW] Consent timeout auto-declines without user notification"
severity: LOW
domain: compliance
lens: consent-flows
labels:
  - consent-timeout
  - user-notification
---

## Summary

When a consent request times out (30 seconds), the system automatically declines the exchange without notifying the user. While this is a reasonable default behavior, the user is not informed that a consent request was pending and timed out.

**Location:** `components/mod_vcard/src/ble_vcard.cpp:1136-1142`

## Impact

1. **Silent failure:** The user may never know that another device tried to exchange vCards if they were looking at a different screen when the request came in.

2. **Lost opportunities:** Users who wanted to accept the exchange but were momentarily distracted lose the chance.

3. **Debugging difficulty:** When users report "nothing happened" during an exchange attempt, it's harder to diagnose if consent timed out silently.

## Evidence

**ble_vcard.cpp:1136-1142** - Consent timeout handler:
```cpp
// Server-side consent timeout
if (s_consent_pending) {
    uint32_t consent_elapsed = now_ms - s_state_start_ms;
    if (consent_elapsed > CONSENT_TIMEOUT_MS) {
        LOG_W(TAG, "Consent timeout, auto-declining");
        ble_vcard_respond_consent(false);  // Auto-declines silently
    }
}
```

**CONSENT_TIMEOUT_MS** is defined at line 185 as 30 seconds:
```cpp
static constexpr uint32_t CONSENT_TIMEOUT_MS = 30000;  // 30 seconds
```

**ble_vcard_respond_consent(false)** sends STATUS_DECLINED to peer but doesn't notify local user:
```cpp
void ble_vcard_respond_consent(bool accepted) {
    ...
    else {
        LOG_I(TAG, "Consent declined");
        sendStatusNotification(s_consent_conn_handle, STATUS_DECLINED);
    }
    s_consent_pending = false;
    // No UI notification to local user!
}
```

## Recommended Fix

Add user notification when consent times out:

1. **Add callback or event** for consent timeout:
```cpp
typedef void (*vcard_consent_timeout_callback_t)(const char* peer_name);
static vcard_consent_timeout_callback_t s_consent_timeout_callback = nullptr;

void ble_vcard_set_consent_timeout_callback(vcard_consent_timeout_callback_t cb) {
    s_consent_timeout_callback = cb;
}
```

2. **Trigger callback on timeout:**
```cpp
if (consent_elapsed > CONSENT_TIMEOUT_MS) {
    LOG_W(TAG, "Consent timeout, auto-declining");
    if (s_consent_timeout_callback) {
        s_consent_timeout_callback(s_consent_peer_name);
    }
    ble_vcard_respond_consent(false);
}
```

3. **Show toast notification** in VcardModule:
```cpp
static void onConsentTimeout(const char* peer_name) {
    static char msg[128];
    snprintf(msg, sizeof(msg), "Exchange from %s timed out", peer_name);
    ui::showToastInfo(msg);
}
```

## References

- ISO/IEC 29100: User notification of privacy-related events
- UX best practices: Inform users of important events even when automatic
