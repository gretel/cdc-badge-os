---
title: "[LOW] CTAP2 makeCredential should return CREDENTIAL_EXCLUDED when credential already exists"
severity: LOW
domain: api-design/api-idempotency
lens: api-idempotency
labels:
  - audit:api-design/api-idempotency
---

## Summary
The CTAP2 `authenticatorMakeCredential` handler checks for existing credentials with the same RP ID + User ID and replaces them (which is FIDO2 spec-compliant), but it doesn't explicitly return `CTAP2_ERR_CREDENTIAL_EXCLUDED` when a credential exists and the client wants to detect duplicates without replacement. This affects clients that rely on this error for idempotent credential creation.

**Location:** `components/mod_fido2/src/ctap2.cpp:1075-1250` (create_credential_and_respond, ctap2_make_credential)

## Impact
- **Client compatibility:** Some FIDO2 clients expect `CREDENTIAL_EXCLUDED` to detect when a credential was already created (for UI feedback)
- **Browser behavior:** Chrome/Firefox use this error to show "Account already registered" messages
- **Current behavior is spec-compliant but not explicit:** The code replaces existing credentials silently

## Evidence
```cpp
// components/mod_fido2/src/ctap2.cpp:762-807
bool fido2_storage_create_credential(
    const char *rp_id, const uint8_t *rp_id_hash,
    const uint8_t *user_id, uint8_t user_id_len,
    const char *user_name, bool resident_key,
    uint8_t cred_protect, uint8_t curve,
    uint8_t *out_slot, uint8_t *out_cred_id, uint8_t *out_pubkey
) {
    // FIDO2 spec: If credential with same RP ID + User ID exists, replace it
    int8_t existing_slot = fido2_storage_find_by_rp_user(rp_id_hash, user_id, user_id_len);
    int8_t slot;

    if (existing_slot >= 0) {
        // Replace existing credential
        LOG_I("FIDO2", "Replacing existing credential in slot %d", existing_slot);
        slot = existing_slot;
        erase_slot_data(static_cast<uint8_t>(slot));
        // Update cache: mark as invalid temporarily
        g_storage.creds[slot].valid = false;
        g_storage.cred_count--;
    } else {
        // Find free slot for new credential
        slot = fido2_storage_find_free_slot();
        if (slot < 0) {
            LOG_E("FIDO2", "No free slots");
            return false;
        }
    }
    // ... creates new credential ...
}
```

The CTAP2 spec defines `0x5E` as `CTAP2_ERR_CREDENTIAL_EXCLUDED` which should be returned when:
> "A credential was already created for the relying party and user."

However, the current implementation always replaces, never returning this error.

## Recommended Fix
Add a mode to detect vs. replace existing credentials:

1. **Check for `options.rk` (resident key)** - if true and credential exists, consider returning excluded
2. **Add parameter** to `fido2_storage_create_credential()` to control replacement behavior
3. **Return `CTAP2_ERR_CREDENTIAL_EXCLUDED`** when appropriate

Example:
```cpp
// In ctap2_make_credential() after parsing options
if (p.rk && existing_slot >= 0) {
    // For resident keys, some clients expect CREDENTIAL_EXCLUDED
    // to indicate the credential already exists
    response[0] = CTAP2_ERR_CREDENTIAL_EXCLUDED;
    *response_len = 1;
    return CTAP2_ERR_CREDENTIAL_EXCLUDED;
}

// For non-resident keys, replacement is fine (current behavior)
```

Alternatively, check the FIDO2 spec's `excludeList` parameter in `MakeCredentialParams`:
```cpp
// If client provided excludeList with credential, and we find a match
if (p.excludeList && find_in_exclude_list(p.excludeList, existing_slot)) {
    response[0] = CTAP2_ERR_CREDENTIAL_EXCLUDED;
    *response_len = 1;
    return CTAP2_ERR_CREDENTIAL_EXCLUDED;
}
```

## References
- [FIDO2 CTAP2 Spec: authenticatorMakeCredential](https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#authenticatorMakeCredential)
- CTAP2 Error Codes: `0x5E` = `CTAP2_ERR_CREDENTIAL_EXCLUDED`
- The `excludeList` parameter in `MakeCredentialParams` is used by clients to avoid duplicate registrations
