---
title: "[LOW] FIDO2 credential enumeration without RP ID limit"
severity: LOW
domain: authentication
lens: auth-session
labels:
  - "audit:security/auth-session"
---

## Summary
The FIDO2 credential enumeration allows iterating through all stored credentials without requiring specific RP ID filtering. This could allow an attacker to enumerate all credentials stored on the device.

**File**: `components/mod_fido2/src/ctap2.cpp:3250-3350`
```cpp
static uint8_t ctap2_get_credential_info(const uint8_t *params, uint16_t params_len,
                                          uint8_t *response, uint16_t *response_len) {
    // Parse parameters
    cbor_reader_t r;
    cbor_reader_init(&r, params, params_len);

    int map_size = cbor_read_map(&r);
    uint8_t index = 0;

    for (int i = 0; i < map_size; i++) {
        cbor_item_t item;
        if (!cbor_read_item(&r, &item)) break;

        int64_t key;
        if (item.type == CBOR_UNSIGNED) {
            key = (int64_t)item.value;
        } else {
            cbor_skip_item(&r);
            continue;
        }

        switch (key) {
            case 0x01:  // credentialIndex
                cbor_read_uint(&r, &index);
                break;
        }
    }

    // Get credential by index
    fido2_credential_info_t info;
    if (!fido2_get_credential_info(index, &info)) {
        return CTAP2_ERR_NO_CREDENTIALS;
    }

    // Build response
    // ...
}
```

The function allows retrieving any credential by index without RP ID verification.

## Impact
- **Credential Enumeration**: An attacker can enumerate all credentials by iterating through indices
- **RP ID Discovery**: Each credential reveals its RP ID hash, allowing an attacker to discover which RPs are stored
- **Privacy Leak**: User could have credentials for multiple services, all of which could be enumerated

## Evidence
**File**: `components/mod_fido2/src/ctap2.cpp:3250-3350`
```cpp
// Get credential by index
fido2_credential_info_t info;
if (!fido2_get_credential_info(index, &info)) {
    return CTAP2_ERR_NO_CREDENTIALS;
}
```

The function retrieves credentials by index without checking if the caller has permission.

**File**: `components/mod_fido2/src/fido2.cpp:215-230`
```cpp
bool fido2_get_credential_info(uint8_t index, fido2_credential_info_t *info) {
    // Map index to slot
    uint8_t found = 0;
    for (uint8_t slot = 0; slot < FIDO2_MAX_CREDENTIALS; slot++) {
        if (fido2_storage_slot_used(slot)) {
            if (found == index) {
                return fido2_storage_get_credential(slot, info);
            }
            found++;
        }
    }
    return false;
}
```

Any index can be queried without authentication.

## Recommended Fix
Require authentication (pinToken) for credential enumeration:

```cpp
static uint8_t ctap2_get_credential_info(const uint8_t *params, uint16_t params_len,
                                          uint8_t *response, uint16_t *response_len) {
    // Check for pinToken
    if (!g_client_pin.pin_token_valid) {
        return CTAP2_ERR_PIN_TOKEN_INVALID;
    }

    // Parse parameters
    cbor_reader_t r;
    cbor_reader_init(&r, params, params_len);

    int map_size = cbor_read_map(&r);
    uint8_t index = 0;
    uint8_t rp_id_hash[32] = {0};
    bool has_rp_id = false;

    for (int i = 0; i < map_size; i++) {
        cbor_item_t item;
        if (!cbor_read_item(&r, &item)) break;

        int64_t key;
        if (item.type == CBOR_UNSIGNED) {
            key = (int64_t)item.value;
        } else {
            cbor_skip_item(&r);
            continue;
        }

        switch (key) {
            case 0x01:  // credentialIndex
                cbor_read_uint(&r, &index);
                break;
            case 0x02:  // rpIdHash
                cbor_read_bytes(&r, rp_id_hash, 32, NULL);
                has_rp_id = true;
                break;
        }
    }

    // Get credential by index and verify RP ID match
    fido2_credential_info_t info;
    if (!fido2_get_credential_info(index, &info)) {
        return CTAP2_ERR_NO_CREDENTIALS;
    }

    // If RP ID specified, verify match
    if (has_rp_id && memcmp(info.rp_id_hash, rp_id_hash, 32) != 0) {
        return CTAP2_ERR_NO_CREDENTIALS;
    }

    // Build response
    // ...
}
```

Also add a limit on how many credentials can be enumerated per session:

```cpp
static struct {
    uint8_t enum_count;
    uint8_t max_enum_per_session;
} g_ctap2 = {};

#define MAX_ENUM_PER_SESSION 10

static uint8_t ctap2_get_credential_info(...) {
    if (g_ctap2.enum_count >= MAX_ENUM_PER_SESSION) {
        return CTAP2_ERR_LIMIT_EXCEEDED;
    }
    g_ctap2.enum_count++;
    // ... rest of code ...
}
```

## References
- FIDO2 CTAP 2.1 Specification - Get Credential Info
- FIDO2 WebAuthn Specification - Credential Discovery
- OWASP Authentication Cheat Sheet - Credential Enumeration

</content>