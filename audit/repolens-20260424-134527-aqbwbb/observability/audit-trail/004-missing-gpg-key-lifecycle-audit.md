---
title: "[MEDIUM] Missing audit events for GPG key lifecycle"
severity: MEDIUM
domain: observability
lens: audit-trail
labels:
  - "audit:observability/audit-trail"
---

## Summary

GPG key generation, export, and reset operations lack structured audit records. The module logs these operations via `LOG_I`/`LOG_D` but without audit-specific fields for tracking key lifecycle, actor identification, and session context.

**Files affected:**
- `components/mod_gpg/src/gpg.cpp:270-350` - `gpg_generate_key()`
- `components/mod_gpg/src/gpg.cpp:400-450` - `gpg_export_pubkey_pem()`
- `components/mod_gpg/src/gpg.cpp:460-500` - `gpg_reset()`
- `components/mod_gpg/src/GpgStorage.cpp:242-320` - `gpg_storage_save_dec_privkey()`

## Impact

1. **Key Tracking**: Cannot reconstruct when keys were generated, replaced, or deleted.
2. **Export Tracking**: No audit of public key exports (important for tracking key distribution).
3. **Reset Tracking**: No record of when GPG module was reset (all keys lost).
4. **Forensic Analysis**: Hard to determine key history for security investigations.

## Evidence

### Key generation logs but doesn't audit

In `gpg.cpp:270-300`:
```cpp
bool gpg_generate_key(uint8_t curve) {
    if (curve != CDC_CURVE_ED25519 && curve != CDC_CURVE_P256) {
        return false;
    }

    // Get current time
    uint32_t now = get_unix_time();

    // Generate key in secure element
    uint8_t sig_slot = gpg_storage_sig_slot();
    if (!se_generate_key(sig_slot, curve)) {
        LOG_E(TAG, "Failed to generate SIG key");
        return false;
    }

    // Generate DEC key and encrypt to R-Memory
    uint8_t dec_slot = gpg_storage_dec_slot();
    if (!se_generate_key(dec_slot, curve)) {
        LOG_E(TAG, "Failed to generate DEC key");
        se_delete_key(sig_slot);
        return false;
    }

    // ... key generation continues ...

    s_metadata.created_at = now;
    s_metadata.curve = curve;
    save_metadata();

    LOG_I(TAG, "Generated %s key at %lu", curve_name, now);
    return true;
}
```

**Missing audit data:**
- No structured record of curve type
- No record of timestamp in audit format
- No session context (USB CCID, BLE?)
- No actor identification
- Log output is volatile

### Key reset lacks audit trail

In `gpg.cpp:460-500`:
```cpp
bool gpg_reset(void) {
    uint8_t sig_slot = gpg_storage_sig_slot();
    uint8_t dec_slot = gpg_storage_dec_slot();
    uint8_t aut_slot = gpg_storage_aut_slot();

    // Delete all keys
    se_delete_key(sig_slot);
    se_delete_key(dec_slot);
    se_delete_key(aut_slot);

    // Delete encrypted DEC key from R-Memory
    gpg_storage_delete_dec_privkey();

    // Clear metadata
    memset(&s_metadata, 0, sizeof(s_metadata));
    save_metadata();

    LOG_I(TAG, "GPG keys reset");
    return true;
}
```

**Missing audit data:**
- No record of which keys were deleted (SIG, DEC, AUT)
- No before-state snapshot (fingerprint, creation time lost)
- No record of who initiated reset

### DEC key save lacks audit

In `GpgStorage.cpp:242-320`:
```cpp
bool gpg_storage_save_dec_privkey(const uint8_t* privkey, const char* pin) {
    // ... validation ...

    // Derive encryption key from PIN
    uint8_t enc_key[32];
    if (!derive_key_from_pin(pin, enc_key)) {
        LOG_E(TAG, "Failed to derive encryption key");
        return false;
    }

    // ... encryption ...

    if (se->rmemWrite(rmem_slot, reinterpret_cast<uint8_t*>(&storage), TOTAL_SIZE)
            != cdc::hal::SeResult::OK) {
        LOG_E(TAG, "Failed to write encrypted DEC key to R-Memory slot %d", rmem_slot);
        goto cleanup;
    }

    LOG_I(TAG, "Saved encrypted DEC private key to R-Memory slot %d", rmem_slot);
    // ...
}
```

**Missing audit data:**
- No record of encryption method (PIN-derived vs device key)
- No record of R-Memory slot used
- No before-state (was this a replacement?)

## Recommended Fix

### Step 1: Add key generation audit (10 min)

Update `gpg_generate_key()`:
```cpp
bool gpg_generate_key(uint8_t curve) {
    // ... existing validation ...

    uint8_t sig_slot = gpg_storage_sig_slot();
    if (!se_generate_key(sig_slot, curve)) {
        core::audit_record(core::AuditEventType::KEY_GENERATE,
                          2, sig_slot, 1, getAuditContext(), curve);
        LOG_E(TAG, "Failed to generate SIG key");
        return false;
    }

    uint8_t dec_slot = gpg_storage_dec_slot();
    if (!se_generate_key(dec_slot, curve)) {
        core::audit_record(core::AuditEventType::KEY_GENERATE,
                          2, dec_slot, 1, getAuditContext(), curve);
        se_delete_key(sig_slot);
        LOG_E(TAG, "Failed to generate DEC key");
        return false;
    }

    // ... success path ...

    s_metadata.created_at = now;
    s_metadata.curve = curve;
    save_metadata();

    // Audit successful generation
    core::audit_record(core::AuditEventType::KEY_GENERATE,
                      2, sig_slot, 0, getAuditContext(),
                      (curve << 8) | (dec_slot << 16) | (aut_slot << 24));

    LOG_I(TAG, "Generated %s key at %lu", curve_name, now);
    return true;
}
```

### Step 2: Add key reset audit (5 min)

Update `gpg_reset()`:
```cpp
bool gpg_reset(void) {
    uint8_t sig_slot = gpg_storage_sig_slot();
    uint8_t dec_slot = gpg_storage_dec_slot();
    uint8_t aut_slot = gpg_storage_aut_slot();

    // Before-state for audit
    uint8_t old_curve = s_metadata.curve;
    uint32_t old_created = s_metadata.created_at;

    // Delete all keys
    se_delete_key(sig_slot);
    se_delete_key(dec_slot);
    se_delete_key(aut_slot);
    gpg_storage_delete_dec_privkey();

    memset(&s_metadata, 0, sizeof(s_metadata));
    save_metadata();

    // Audit reset with before-state
    core::audit_record(core::AuditEventType::KEY_GENERATE,  // Reuse as KEY_DELETE
                      2, sig_slot, 0, getAuditContext(),
                      (old_curve << 8) | (old_created & 0xFFFF));

    LOG_I(TAG, "GPG keys reset");
    return true;
}
```

### Step 3: Add DEC key save audit (5 min)

Update `gpg_storage_save_dec_privkey()`:
```cpp
bool gpg_storage_save_dec_privkey(const uint8_t* privkey, const char* pin) {
    // ... existing code ...

    // Audit save
    uint8_t context = getAuditContext();
    uint8_t result = 0;  // Will set to 1 on failure
    uint16_t rmem_slot = s_storage.rmemStart + RMEM_SLOT_DEC_KEY;

    bool success = true;
    // ... encryption and write ...
    if (se->rmemWrite(rmem_slot, reinterpret_cast<uint8_t*>(&storage), TOTAL_SIZE)
            != cdc::hal::SeResult::OK) {
        result = 1;
        success = false;
        LOG_E(TAG, "Failed to write encrypted DEC key to R-Memory slot %d", rmem_slot);
        goto cleanup;
    }

    core::audit_record(core::AuditEventType::KEY_GENERATE,  // Reuse as KEY_SAVE
                      2, static_cast<uint8_t>(rmem_slot), result,
                      context, (pin ? 0 : 1) << 8);  // 0 = PIN-derived, 1 = device key

    LOG_I(TAG, "Saved encrypted DEC private key to R-Memory slot %d", rmem_slot);
    // ...
}
```

## References

- OpenPGP Card Spec Section 5 (Key Generation)
- NIST SP 800-73-4 (PIV) - key lifecycle tracking
- Common Criteria EAL2+ - FAU_GEN.1 audit data generation
