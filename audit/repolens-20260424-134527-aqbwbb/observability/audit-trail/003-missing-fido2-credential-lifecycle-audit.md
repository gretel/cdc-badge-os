---
title: "[HIGH] Missing audit events for FIDO2 credential lifecycle"
severity: HIGH
domain: observability
lens: audit-trail
labels:
  - "audit:observability/audit-trail"
---

## Summary

FIDO2 credential creation, deletion, and sign-count updates lack structured audit records. The storage layer logs these operations but without audit-specific fields for tracking credential lifecycle, making forensic analysis and compliance reporting difficult.

**Files affected:**
- `components/mod_fido2/src/fido2_storage.cpp:762-888` - `fido2_storage_create_credential()`
- `components/mod_fido2/src/fido2_storage.cpp:895-915` - `fido2_storage_delete_credential()`
- `components/mod_fido2/src/fido2_storage.cpp:920-942` - `fido2_storage_increment_sign_count()`
- `components/mod_fido2/src/ctap2.cpp:1075-1169` - `create_credential_and_respond()`

## Impact

1. **Credential Tracking**: Cannot reconstruct which credentials existed and when they were created/deleted.
2. **Sign Count Verification**: No audit trail for sign count increments (important for replay attack detection).
3. **FIDO2 Certification**: Requires audit of credential management operations.
4. **Forensic Analysis**: Hard to determine if credential loss was due to deletion, corruption, or storage failure.

## Evidence

### Credential creation logs but doesn't audit

In `fido2_storage.cpp:805-888`:
```cpp
LOG_I("FIDO2", "Creating %s credential in slot %d for %s", curve_name, slot, rp_id);

// Explicitly erase ECC slot first to ensure it's empty
LOG_D("FIDO2", "Erasing slot %d before key generation", slot);
uint8_t phys_slot = ecc_slot_for_logical(static_cast<uint8_t>(slot));
auto* se = get_se();
if (!se) return false;
se->eccDelete(phys_slot);

// Generate ECC key with requested curve
cdc::hal::EccCurve se_curve =
    (curve == CDC_CURVE_ED25519) ? cdc::hal::EccCurve::ED25519
                                 : cdc::hal::EccCurve::P256;
if (se->eccGenerate(phys_slot, se_curve) != cdc::hal::SeResult::OK) {
    LOG_E("FIDO2", "Failed to generate %s key in slot %d", curve_name, slot);
    return false;
}

// ... key generation continues ...

// Write to R-Memory
if (!write_rmem_credential(static_cast<uint8_t>(slot), &stored)) {
    se->eccDelete(phys_slot);
    return false;
}

LOG_I("FIDO2", "Created %s credential in slot %d", curve_name, slot);
return true;
```

**Missing audit data:**
- No structured record of RP ID hash (for correlating with specific relying parties)
- No record of user ID (for tracking which user handle was created)
- No before/after state (for tracking replacements)
- No session context (USB HID, BLE?)
- No timestamp in audit format

### Credential deletion lacks audit trail

In `fido2_storage.cpp:895-915`:
```cpp
bool fido2_storage_delete_credential(uint8_t slot) {
    if (!slot_logical_valid(slot) || !g_storage.creds[slot].valid) {
        return false;
    }

    LOG_I("FIDO2", "Deleting credential in slot %d", slot);

    // Erase ECC key and R-Memory
    erase_slot_data(slot);

    // Update local cache
    g_storage.creds[slot].valid = false;
    g_storage.cred_count--;

    LOG_I("FIDO2", "Deleted credential in slot %d", slot);
    return true;
}
```

**Missing audit data:**
- No record of which RP ID was deleted (only slot number)
- No record of who initiated deletion
- No before-state snapshot (user name, sign count lost forever)

### Sign count increment not audited

In `fido2_storage.cpp:920-942`:
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

    return new_count;
}
```

**Missing audit data:**
- No record of old vs. new sign count
- No correlation to assertion event that triggered increment
- No timestamp for when increment occurred

## Recommended Fix

### Step 1: Add credential create audit (15 min)

Update `fido2_storage_create_credential()`:
```cpp
bool fido2_storage_create_credential(...) {
    // ... existing validation ...

    int8_t existing_slot = fido2_storage_find_by_rp_user(rp_id_hash, user_id, user_id_len);
    int8_t slot;

    if (existing_slot >= 0) {
        LOG_I("FIDO2", "Replacing existing credential in slot %d", existing_slot);
        slot = existing_slot;
        erase_slot_data(static_cast<uint8_t>(slot));
        g_storage.creds[slot].valid = false;
        g_storage.cred_count--;

        // Audit replacement
        core::audit_record(core::AuditEventType::CREDENTIAL_DELETE,
                          1, static_cast<uint8_t>(slot), 0, 0, stored.sign_count);
    } else {
        slot = fido2_storage_find_free_slot();
        if (slot < 0) {
            LOG_E("FIDO2", "No free slots");
            return false;
        }
    }

    // ... key generation ...

    // Write to R-Memory
    if (!write_rmem_credential(static_cast<uint8_t>(slot), &stored)) {
        se->eccDelete(phys_slot);
        return false;
    }

    // Update local cache
    update_cache_from_stored(static_cast<uint8_t>(slot), &stored, resident_key);
    g_storage.cred_count++;

    // Audit creation
    core::audit_record(core::AuditEventType::CREDENTIAL_CREATE,
                      1, static_cast<uint8_t>(slot), 0, 0,
                      (resident_key ? 1 : 0) | (curve << 8));

    LOG_I("FIDO2", "Created %s credential in slot %d", curve_name, slot);
    return true;
}
```

### Step 2: Add credential delete audit (10 min)

Update `fido2_storage_delete_credential()`:
```cpp
bool fido2_storage_delete_credential(uint8_t slot) {
    if (!slot_logical_valid(slot) || !g_storage.creds[slot].valid) {
        return false;
    }

    // Read before-state for audit
    fido2_stored_cred_t stored;
    read_rmem_credential(slot, &stored);

    LOG_I("FIDO2", "Deleting credential in slot %d", slot);
    erase_slot_data(slot);

    g_storage.creds[slot].valid = false;
    g_storage.cred_count--;

    // Audit deletion with before-state
    core::audit_record(core::AuditEventType::CREDENTIAL_DELETE,
                      1, slot, 0, 0, stored.sign_count);

    LOG_I("FIDO2", "Deleted credential in slot %d", slot);
    return true;
}
```

### Step 3: Add sign count audit (5 min)

Update `fido2_storage_increment_sign_count()`:
```cpp
uint32_t fido2_storage_increment_sign_count(uint8_t slot) {
    if (!slot_logical_valid(slot) || !g_storage.creds[slot].valid) {
        return 0;
    }

    g_storage.creds[slot].sign_count++;
    uint32_t new_count = g_storage.creds[slot].sign_count;

    fido2_stored_cred_t stored;
    if (read_rmem_credential(slot, &stored)) {
        stored.sign_count = new_count;
        if (!write_rmem_credential(slot, &stored)) {
            LOG_E("FIDO2", "CRITICAL: Failed to persist sign count for slot %d!", slot);
        }
    }

    // Audit sign count increment
    core::audit_record(core::AuditEventType::KEY_GENERATE,  // Reuse as SIGN_COUNT_INCREMENT
                      1, slot, 0, 0, (new_count << 16) | stored.sign_count);

    return new_count;
}
```

## References

- FIDO2 CTAP2 Spec Section 6.1 (Authenticator Data) - sign count tracking
- NIST FIDO2 Certification Requirements - credential lifecycle audit
- Common Criteria EAL2+ - FAU_GEN.1 audit data generation
