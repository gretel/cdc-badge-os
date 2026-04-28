---
title: "[HIGH] Missing audit trail for FIDO2 credential operations"
severity: HIGH
domain: fido2-authentication
lens: audit-trail
labels:
  - audit:observability/audit-trail
---

## Summary
FIDO2 credential creation, deletion, and signing operations lack audit logging. The storage layer in `components/mod_fido2/src/fido2_storage.cpp` performs critical state changes without generating structured audit records.

**Missing audit events:**
1. Credential creation (`fido2_storage_create_credential`)
2. Credential deletion (`fido2_storage_delete_credential`)
3. Sign counter increments (`fido2_storage_increment_sign_count`)
4. Global auth counter increments (`fido2_storage_counter_increment`)

## Impact
- **Compliance**: FIDO2 requires sign counter tracking for replay attack detection; without audit logs, counter progression is invisible
- **Security**: Cannot detect unauthorized credential additions or deletions
- **Forensics**: If credentials are compromised, no trail exists to determine when or how many times each was used
- **Accountability**: No record of which Relying Party (RP) credentials were created for

## Evidence
```cpp
// fido2_storage.cpp:814 - Credential creation
bool fido2_storage_create_credential(...) {
    // ... key generation ...
    if (!write_rmem_credential(static_cast<uint8_t>(slot), &stored)) {
        se->eccDelete(phys_slot);
        return false;
    }
    update_cache_from_stored(static_cast<uint8_t>(slot), &stored, resident_key);
    g_storage.cred_count++;
    LOG_I("FIDO2", "Created %s credential in slot %d", curve_name, slot);
    return true;  // No audit event!
}

// fido2_storage.cpp:898 - Credential deletion
bool fido2_storage_delete_credential(uint8_t slot) {
    erase_slot_data(slot);
    g_storage.creds[slot].valid = false;
    g_storage.cred_count--;
    LOG_I("FIDO2", "Deleted credential in slot %d", slot);
    return true;  // No audit event!
}

// fido2_storage.cpp:919 - Sign counter increment
uint32_t fido2_storage_increment_sign_count(uint8_t slot) {
    g_storage.creds[slot].sign_count++;
    uint32_t new_count = g_storage.creds[slot].sign_count;
    // ... persist to storage ...
    return new_count;  // No audit event for counter change!
}
```

## Recommended Fix
1. Create audit helper in `components/mod_fido2/include/mod_fido2/fido2_audit.h`:
   ```cpp
   void fido2_audit_credential_created(const char* rp_id, const char* user_name, uint8_t slot, bool resident);
   void fido2_audit_credential_deleted(uint8_t slot);
   void fido2_audit_sign(uint8_t slot, uint32_t new_sign_count);
   void fido2_audit_auth_counter_increment(uint32_t new_count);
   ```

2. Add calls to existing functions:
   - After successful `write_rmem_credential()` in `fido2_storage_create_credential()`
   - After `erase_slot_data()` in `fido2_storage_delete_credential()`
   - After sign count update in `fido2_storage_increment_sign_count()`
   - After NVS commit in `fido2_storage_counter_increment()`

3. Log format should include:
   - Event type (CREATE/DELETE/SIGN)
   - Timestamp (from RTC or boot time)
   - Slot number
   - RP ID (for create)
   - User name (for create)
   - Sign count (for sign)

## References
- FIDO CTAP2 Specification (Section 6.1 Sign Counter)
- FIDO Alliance Audit Considerations Whitepaper
- NIST SP 800-63B Digital Identity Guidelines (Section 5.3.3)
