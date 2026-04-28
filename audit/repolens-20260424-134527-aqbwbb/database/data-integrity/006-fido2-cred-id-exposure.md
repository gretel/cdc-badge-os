---
title: "[MEDIUM] FIDO2 credential ID format exposes internal slot structure"
severity: MEDIUM
domain: data-integrity
lens: database
labels:
  - fido2-storage
  - credential-id
  - encoding
---

## Summary
The FIDO2 credential ID is constructed as: `[slot index (1 byte)] + [random nonce (16 bytes)] + [padding (47 bytes)]`. The slot index is stored in plain form at the beginning of the credential ID, making it easy to determine which internal slot a credential uses. While this isn't a critical security issue, it violates the principle that credential IDs should be opaque.

**Files:**
- `components/mod_fido2/src/fido2_storage.cpp:684-698` (get_cred_id function)
- `components/mod_fido2/src/fido2_storage.cpp:836-847` (create_credential function)

## Impact
The credential ID format reveals internal structure:
- Anyone with a credential ID can determine the slot index (first byte)
- Makes it easier to correlate credentials across different contexts
- If slot allocation becomes predictable, could aid side-channel analysis
- Per FIDO2 spec, credential IDs should be "opaque to the client"

## Evidence
From `fido2_storage.cpp:684-698`:
```cpp
bool fido2_storage_get_cred_id(uint8_t slot, uint8_t *out_cred_id) {
    if (!slot_logical_valid(slot) || !g_storage.creds[slot].valid || !out_cred_id) {
        return false;
    }

    fido2_stored_cred_t stored;
    if (!read_rmem_credential(slot, &stored)) {
        LOG_E("FIDO2", "Failed to read credential %d", slot);
        return false;
    }

    // Build credential ID: slot (1) + nonce (16) + padding (47) = 64 bytes
    memset(out_cred_id, 0, FIDO2_CRED_ID_LEN);
    out_cred_id[0] = slot;  // PLAIN slot index!
    memcpy(out_cred_id + 1, stored.cred_id_nonce, 16);

    return true;
}
```

From `fido2_storage.cpp:836-847`:
```cpp
// Generate random nonce for credential ID
uint8_t nonce[16];
if (!se->getRandom(nonce, 16)) {
    LOG_E("FIDO2", "Failed to generate nonce");
    se->eccDelete(phys_slot);
    return false;
}

// Build credential ID (64 bytes)
// Format: slot (1) + nonce (16) + padding (47)
// In production, use HMAC for binding
memset(out_cred_id, 0, FIDO2_CRED_ID_LEN);
out_cred_id[0] = slot;
memcpy(out_cred_id + 1, nonce, 16);
```

Note the comment at line 842: `// In production, use HMAC for binding` - this acknowledges the current format is simplified.

## Recommended Fix
Use an HMAC-based credential ID format as suggested in the code comment:

```cpp
// Better credential ID format:
// [slot (1)] + [HMAC-SHA256(slot + nonce + rp_id)[:31]] + [padding]

uint8_t hmac[32];
mbedtls_hmac_context_t ctx;
mbedtls_hmac_init(&ctx);
mbedtls_hmac_starts(&ctx, secret_key, 32);
mbedtls_hmac_update(&ctx, &slot, 1);
mbedtls_hmac_update(&ctx, nonce, 16);
mbedtls_hmac_update(&ctx, rp_id_hash, 32);
mbedtls_hmac_finish(&ctx, hmac);
mbedtls_hmac_free(&ctx);

// Build credential ID
out_cred_id[0] = slot;
memcpy(out_cred_id + 1, hmac, 31);  // First 31 bytes of HMAC
// Rest is zero-padded
```

This makes the credential ID:
1. Still contain slot index for quick lookup
2. Bind to the RP ID (prevents credential reuse across RPs)
3. Be less predictable (HMAC output is cryptographically random)
4. Allow verification that credential ID matches slot

Alternatively, use a simpler approach:
```cpp
// Just hash the nonce + slot together
uint8_t hash[32];
sha256(slot + nonce, &hash);
out_cred_id[0] = slot;
memcpy(out_cred_id + 1, hash, 31);
```

## References
- FIDO2 Credential ID: https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#key-credential-management
- CTAP2 spec: Credential IDs should be opaque to relying parties
- WebAuthn spec: "The credential ID is an opaque byte sequence"
