---
title: "[MEDIUM] FIDO2 getAssertion response includes user ID and name"
severity: MEDIUM
domain: mod_fido2
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The `authenticatorGetAssertion` response in `components/mod_fido2/src/ctap2.cpp:1728-1741` includes the user entity map (user ID bytes and user name) when returning credentials for discoverable credentials. This user data is sent over USB HID to the host computer as part of the CBOR-encoded response.

## Impact
- **User Data Leakage**: Each authentication response includes the user ID (raw bytes) and user name (display name)
- **Cross-Session Tracking**: User IDs can be used to track the same user across different authentication sessions
- **PII Exposure**: User names (e.g., "John Doe", email addresses) are transmitted in plaintext
- **Redundant to credMgmt**: Related to finding #014, but affects the authentication flow instead of credential management

## Evidence
File: `components/mod_fido2/src/ctap2.cpp:1728-1741`

```cpp
// 0x04: user (only for discoverable credentials)
if (include_user) {
    cbor_encode_map(&w, user_fields);
    
    cbor_encode_text(&w, "id");
    cbor_encode_bytes(&w, cred->user_id, cred->user_id_len);  // RAW USER ID BYTES
    
    if (cred->user_name[0] != '\0') {
        cbor_encode_text(&w, "name");
        cbor_encode_text(&w, cred->user_name);  // USER DISPLAY NAME
    }
}
```

This function is called from the getAssertion handler at line 1881:
```cpp
status = ga_build_response(cred_id, auth_data, auth_data_len, signature, sig_len,
                           &info, true, 1, response, response_len);
```

The `include_user` parameter is set to `true` for discoverable credentials, causing user data to be included in every authentication response.

## Recommended Fix
1. **Add a configuration flag** to control user entity inclusion (e.g., `FEATURE_FIDO2_USER_ENTITY`)
2. **Mask user ID** - send only a truncated hash or first few bytes
3. **Mask user name** - show only initials (e.g., "J. D.")
4. **Follow FIDO2 spec** - user entity is optional in getAssertion responses

Example fix:
```cpp
// 0x04: user (only for discoverable credentials)
if (include_user) {
#if FEATURE_FIDO2_USER_ENTITY
    cbor_encode_map(&w, user_fields);
    cbor_encode_text(&w, "id");
    cbor_encode_bytes(&w, cred->user_id, cred->user_id_len);
    if (cred->user_name[0] != '\0') {
        cbor_encode_text(&w, "name");
        cbor_encode_text(&w, cred->user_name);
    }
#else
    // Omit user entity entirely for privacy
    include_user = false;
#endif
}
```

## References
- FIDO2 CTAP2 specification: authenticatorGetAssertion
- FIDO2 WebAuthn specification: Authenticator Data
- OWASP: [User Enumeration](https://owasp.org/www-community/attacks/User_Enumeration)
- Related finding: #014 (credMgmt user data exposure)

</content>