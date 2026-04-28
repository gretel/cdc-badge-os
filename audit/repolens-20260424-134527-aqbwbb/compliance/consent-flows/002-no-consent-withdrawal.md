---
title: "[MEDIUM] No mechanism to withdraw previously granted consent"
severity: MEDIUM
domain: compliance
lens: consent-flows
labels:
  - consent-withdrawal
---

## Summary

The vCard BLE exchange module (`components/mod_vcard/`) allows users to accept or decline incoming exchange requests, but there is **no mechanism to withdraw previously granted consent**. Once a peer is accepted, the user cannot easily revoke that permission.

**Location:** `components/mod_vcard/src/ble_vcard.cpp` - Consent response function
**Location:** `components/mod_vcard/src/VcardModule.cpp` - UI menu and callbacks

## Impact

1. **No "right to withdraw":** GDPR Article 7(3) requires that "It shall be as easy to withdraw as to give consent." Currently, there is no UI or API to:
   - View list of peers with active consent
   - Revoke consent for a specific peer
   - Clear all consent history

2. **Permanent data access:** Once a peer's vCard is exchanged and stored, there is no consent-based mechanism to limit future access or data retention.

3. **User control limitation:** Users cannot review and manage their consent preferences from a central location.

## Evidence

The consent flow only handles the initial decision:

**ble_vcard.cpp:1067-1079** - Only accepts or declines in the moment:
```cpp
void ble_vcard_respond_consent(bool accepted) {
    if (!s_consent_pending || s_consent_conn_handle == INVALID_HANDLE) return;

    if (accepted) {
        s_receive_enabled = true;  // One-time enable
        sendStatusNotification(s_consent_conn_handle, STATUS_ACCEPTED);
    } else {
        sendStatusNotification(s_consent_conn_handle, STATUS_DECLINED);
    }

    s_consent_pending = false;  // Decision processed, no further management
}
```

**VcardModule.cpp:131-166** - UI only shows consent prompt, no management view:
```cpp
static void onConsentAccept(void* userData) {
    ble_vcard_respond_consent(true);
    ui::ViewStack::instance().pop();
    ui::showToastInfo("Waiting for vCard...");
}

static void onConsentDecline(void* userData) {
    ble_vcard_respond_consent(false);
    ui::ViewMain::instance().pop();
}
```

No functions exist to:
- List peers with active consent
- Revoke consent for a specific peer
- Clear all consent history

## Recommended Fix

Add consent withdrawal functionality:

1. **Add API functions:**
```cpp
// Revoke consent for specific peer
bool ble_vcard_revoke_consent(const uint8_t* peer_addr);

// Get list of peers with active consent
uint16_t ble_vcard_get_consent_peers(vcard_peer_t* out, uint16_t max_peers);

// Clear all consent history
void ble_vcard_clear_all_consent(void);
```

2. **Add UI menu item** in vCard module settings:
   - "Manage Consent" → Shows list of peers
   - Each peer has "Revoke" option

3. **Update vcard_store** to track consent state alongside stored vCards

## References

- GDPR Article 7(3): "The data subject shall have the right to withdraw his or her consent at any time"
- ISO/IEC 29100: Privacy framework - Consent revocation requirements
- ePrivacy Directive: User control over data sharing preferences
