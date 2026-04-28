---
title: "[HIGH] Missing audit trail for FIDO2 sign operations"
severity: HIGH
domain: fido2-authentication
lens: audit-trail
labels:
  - audit:observability/audit-trail
---

## Summary

FIDO2 sign operations (authentication assertions) are performed but lack audit logging. When `fido2_storage_sign()` or `fido2_storage_sign_raw()` is called to generate a signature for a Relying Party, no structured audit record is created. This differs from credential lifecycle tracking (already covered in existing findings) - this is about tracking **when** credentials are used for authentication.

**Files affected:**
- `components/mod_fido2/src/fido2_storage.cpp:950` - `fido2_storage_sign()` - Signs message hash with ECDSA
- `components/mod_fido2/src/fido2_storage.cpp:981` - `fido2_storage_sign_raw()` - Signs message with EdDSA/ECDSA
- `components/mod_fido2/src/ctap2.cpp:1842` - Sign counter incremented during assertion
- `components/mod_fido2/src/ctap2.cpp:1925` - Sign counter incremented for getAssertion

**Missing audit events:**
1. Sign operation started (with RP ID, credential slot)
2. Sign operation completed (with new sign count)
3. Sign operation failed (with error reason)

## Impact

1. **Replay attack detection**: FIDO2 spec requires sign counter tracking. Without audit logs, you cannot verify counter progression over time.
2. **Usage forensics**: Cannot determine when a specific credential was last used for authentication.
3. **Security investigations**: If a credential is compromised, no trail exists to determine how many times it was used.
4. **Compliance**: FIDO2 certification may require audit trails for authentication operations.
5. **Accountability**: No record of which credentials were used for which Relying Parties.

## Evidence

### Sign operation lacks audit trail

In `fido2_storage.cpp:950-970`:
```cpp
bool fido2_storage_sign(uint8_t slot, const uint8_t *msg, uint16_t msg_len,
                        uint8_t *signature, uint8_t *sig_len) {
    if (!slot_logical_valid(slot) || !g_storage.creds[slot].valid) {
        return false;
    }

    // ECDSA sign: hash message and sign
    uint8_t hash[32];
    sha256(msg, msg_len, hash);

    uint8_t raw_sig[64];
    if (!ecdsa_sign_hash(slot, hash, raw_sig)) {
        return false;
    }

    // Convert to DER format
    *sig_len = raw_sig_to_der(raw_sig, signature);

    LOG_D("FIDO2", "Signed with slot %d, sig_len=%d", slot, *sig_len);  // Basic log only
    return true;  // No audit event!
}
```

### Sign counter increment lacks audit trail

In `fido2_storage.cpp:918-937`:
```cpp
uint32_t fido2_storage_increment_sign_count(uint8_t slot) {
    if (!slot_logical_valid(slot) || !g_storage.creds[slot].valid) {
        return 0;
    }

    // Increment local cache
    g_storage.creds[slot].sign_count++;
    uint32_t new_count = g_storage.creds[slot].sign_count;

    // Read current stored data from TROPIC01
    fido2_stored_cred_t stored;
    if (read_rmem_credential(slot, &stored)) {
        stored.sign_count = new_count;
        if (!write_rmem_credential(slot, &stored)) {
            LOG_E("FIDO2", "CRITICAL: Failed to persist sign count for slot %d!", slot);
        }
    }

    return new_count;  // No audit event for counter change!
}
```

### Sign operation called from CTAP2 without audit

In `ctap2.cpp:1842-1881`:
```cpp
uint32_t sign_count = fido2_storage_increment_sign_count(slot);
// ... build authenticator data ...
// Step 9: Generate signature
uint8_t signature[128];
if (!fido2_storage_sign_raw(slot, to_sign, to_sign_len, signature, &sig_len)) {
    // ...
}
// Build response with signature
status = ga_build_response(cred_id, auth_data, auth_data_len, signature, sig_len, ...);
```

The sign operation happens but no audit record is created with:
- RP ID (who authenticated)
- Credential slot (which key)
- Sign count (counter value)
- Timestamp (when)
- Success/failure status

## Recommended Fix

### Step 1: Create audit helper for FIDO2 sign operations (20 min)

Create `components/mod_fido2/include/mod_fido2/fido2_sign_audit.h`:

```cpp
#ifndef MOD_FIDO2_FIDO2_SIGN_AUDIT_H
#define MOD_FIDO2_FIDO2_SIGN_AUDIT_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Audit record for FIDO2 sign operation.
 */
void fido2_audit_sign_started(uint8_t slot, const char* rp_id);

/**
 * \brief Audit record for successful sign operation.
 * \param slot Credential slot used.
 * \param sign_count New sign counter value.
 * \param rp_id Relying Party ID.
 */
void fido2_audit_sign_success(uint8_t slot, uint32_t sign_count, const char* rp_id);

/**
 * \brief Audit record for failed sign operation.
 * \param slot Credential slot.
 * \param rp_id Relying Party ID.
 * \param error_code Error code (0=success, 1=invalid slot, 2=sign failed, etc).
 */
void fido2_audit_sign_failure(uint8_t slot, const char* rp_id, uint8_t error_code);

#ifdef __cplusplus
}
#endif

#endif // MOD_FIDO2_FIDO2_SIGN_AUDIT_H
```

### Step 2: Implement audit functions in `fido2_storage.cpp` (15 min)

Add to `components/mod_fido2/src/fido2_storage.cpp`:

```cpp
#include "mod_fido2/fido2_sign_audit.h"

static void fido2_audit_sign_started(uint8_t slot, const char* rp_id) {
    fido2_stored_cred_t stored;
    if (read_rmem_credential(slot, &stored)) {
        LOG_I("FIDO2", "AUDIT: SIGN START | slot=%d | rp=%s | counter=%lu",
              slot, rp_id ? rp_id : "unknown", (unsigned long)stored.sign_count);
    } else {
        LOG_I("FIDO2", "AUDIT: SIGN START | slot=%d | rp=%s",
              slot, rp_id ? rp_id : "unknown");
    }
}

static void fido2_audit_sign_success(uint8_t slot, uint32_t sign_count, const char* rp_id) {
    LOG_I("FIDO2", "AUDIT: SIGN OK | slot=%d | rp=%s | counter=%lu",
          slot, rp_id ? rp_id : "unknown", (unsigned long)sign_count);
}

static void fido2_audit_sign_failure(uint8_t slot, const char* rp_id, uint8_t error_code) {
    LOG_I("FIDO2", "AUDIT: SIGN FAIL | slot=%d | rp=%s | err=%d",
          slot, rp_id ? rp_id : "unknown", error_code);
}
```

### Step 3: Integrate audit calls into sign operations (15 min)

Update `fido2_storage_sign()`:

```cpp
bool fido2_storage_sign(uint8_t slot, const uint8_t *msg, uint16_t msg_len,
                        uint8_t *signature, uint8_t *sig_len) {
    if (!slot_logical_valid(slot) || !g_storage.creds[slot].valid) {
        fido2_audit_sign_failure(slot, NULL, 1);  // Invalid slot
        return false;
    }

    // Get RP ID from credential for audit
    fido2_stored_cred_t stored;
    read_rmem_credential(slot, &stored);
    fido2_audit_sign_started(slot, stored.rp_id);

    // ECDSA sign: hash message and sign
    uint8_t hash[32];
    sha256(msg, msg_len, hash);

    uint8_t raw_sig[64];
    if (!ecdsa_sign_hash(slot, hash, raw_sig)) {
        fido2_audit_sign_failure(slot, stored.rp_id, 2);  // Sign failed
        return false;
    }

    // Convert to DER format
    *sig_len = raw_sig_to_der(raw_sig, signature);

    fido2_audit_sign_success(slot, g_storage.creds[slot].sign_count, stored.rp_id);
    LOG_D("FIDO2", "Signed with slot %d, sig_len=%d", slot, *sig_len);
    return true;
}
```

Update `fido2_storage_increment_sign_count()`:

```cpp
uint32_t fido2_storage_increment_sign_count(uint8_t slot) {
    if (!slot_logical_valid(slot) || !g_storage.creds[slot].valid) {
        return 0;
    }

    // Get RP ID before increment
    fido2_stored_cred_t stored;
    read_rmem_credential(slot, &stored);

    // Increment local cache
    g_storage.creds[slot].sign_count++;
    uint32_t new_count = g_storage.creds[slot].sign_count;

    // Read current stored data from TROPIC01
    if (read_rmem_credential(slot, &stored)) {
        stored.sign_count = new_count;
        if (!write_rmem_credential(slot, &stored)) {
            LOG_E("FIDO2", "CRITICAL: Failed to persist sign count for slot %d!", slot);
        }
    }

    return new_count;
}
```

### Step 4: Add audit call in CTAP2 assertion flow (10 min)

In `ctap2.cpp` after successful assertion:

```cpp
// After ga_build_response succeeds
uint32_t sign_count = fido2_storage_increment_sign_count(slot);
// ...
status = ga_build_response(cred_id, auth_data, auth_data_len, signature, sig_len, ...);
// Add audit:
fido2_audit_sign_success(slot, sign_count, g_ctap2.assertion_rp_id);
```

## References

- FIDO CTAP2 Specification (Section 6.1 Sign Counter) - requires tracking sign operations
- FIDO2 Server Implementation Best Practices (Section 6.3 Audit Logging)
- NIST SP 800-63B Digital Identity Guidelines (Section 5.3.3 Authentication)
- Related finding: `002-missing-fido2-credential-audit.md` covers credential lifecycle, this covers sign operations

</content>