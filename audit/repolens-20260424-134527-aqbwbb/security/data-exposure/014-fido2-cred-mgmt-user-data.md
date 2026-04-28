---
title: "[MEDIUM] FIDO2 credential management exposes user IDs and names in CBOR responses"
severity: MEDIUM
domain: mod_fido2
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The FIDO2 credential management command (`credMgmt`) in `components/mod_fido2/src/ctap2.cpp:3061-3082` encodes and returns user IDs (raw bytes) and user names (display names) as part of the CBOR response for each credential. This information is sent over the USB HID interface to the host, potentially exposing user metadata to any connected computer.

## Impact
- **User Enumeration**: The `getCredentials` response reveals all stored user IDs and names for a given RP, allowing an attacker to enumerate accounts
- **PII Exposure**: User names (e.g., "John Doe", email addresses) are transmitted in plaintext over USB
- **User ID Leakage**: Raw user ID bytes (often containing structured data like UUIDs or database IDs) are exposed
- **Cross-Device Tracking**: User IDs can be used to track the same user across different relying parties

## Evidence
File: `components/mod_fido2/src/ctap2.cpp:3061-3082`

```cpp
static void cred_mgmt_encode_credential(cbor_writer_t *w, uint8_t slot, bool include_total) {
    fido2_credential_info_t info;
    if (!fido2_storage_get_credential(slot, &info)) return;

    // ...

    // 0x06: user
    cbor_encode_map(w, info.user_name[0] ? 2 : 1);
    cbor_encode_text(w, "id");
    cbor_encode_bytes(w, info.user_id, info.user_id_len);  // RAW USER ID BYTES
    if (info.user_name[0]) {
        cbor_encode_text(w, "name");
        cbor_encode_text(w, info.user_name);  // USER DISPLAY NAME
    }

    // ...
}
```

This function is called from the `credMgmt` command handler at line 3330:
```cpp
cred_mgmt_encode_credential(&w, g_cred_mgmt.cred_slots[g_cred_mgmt.cred_index], false);
```

The response is sent via USB HID to the host computer, which may log or process this data.

## Recommended Fix
1. **Add a configuration flag** to control whether user metadata is included in responses (e.g., `FEATURE_FIDO2_USER_METADATA`)
2. **Mask user IDs** - send only a truncated hash of the user ID instead of raw bytes
3. **Mask user names** - show only initials or first character (e.g., "J. D." instead of "John Doe")
4. **Add opt-in behavior** - only include user metadata when explicitly requested by the authenticator

Example fix:
```cpp
#if FEATURE_FIDO2_USER_METADATA
    // 0x06: user (full details)
    cbor_encode_map(w, info.user_name[0] ? 2 : 1);
    cbor_encode_text(w, "id");
    cbor_encode_bytes(w, info.user_id, info.user_id_len);
    if (info.user_name[0]) {
        cbor_encode_text(w, "name");
        cbor_encode_text(w, info.user_name);
    }
#else
    // 0x06: user (masked)
    cbor_encode_map(w, 1);
    cbor_encode_text(w, "id");
    cbor_encode_bytes(w, info.user_id, 4);  // Only first 4 bytes
    if (info.user_name[0]) {
        cbor_encode_text(w, "name");
        char masked[4];
        snprintf(masked, sizeof(masked), "%c.", info.user_name[0]);
        cbor_encode_text(w, masked);
    }
#endif
```

## References
- FIDO2 CTAP2 specification: Credential Management
- OWASP: [User Enumeration](https://owasp.org/www-community/attacks/User Enumeration)
- FIDO2 Best Practices: [User Handle Privacy](https://fidoalliance.org/specs/fido-v2.1-ps-20210615/fido-v2.1-ps-20210615.html#user-handles)

</content>