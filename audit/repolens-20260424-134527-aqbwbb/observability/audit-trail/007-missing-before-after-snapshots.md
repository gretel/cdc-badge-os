---
title: "[LOW] Missing before/after snapshots for state changes"
severity: LOW
domain: observability
lens: audit-trail
labels:
  - "audit:observability/audit-trail"
---

## Summary

When state changes occur (PIN updates, credential replacements, key regeneration), no before-state snapshots are recorded in the audit trail. This makes it impossible to reconstruct what changed and to what value.

**Files affected:**
- `components/cdc_core/src/PinManager.cpp` - PIN change operations
- `components/mod_fido2/src/fido2_storage.cpp` - Credential create/replace
- `components/mod_gpg/src/gpg.cpp` - Key generation/reset

## Impact

1. **Forensic Analysis**: Cannot determine what value changed to what new value.
2. **Debugging**: Hard to track down unintended state changes.
3. **Compliance**: Some standards require before/after snapshots for critical changes.

## Evidence

### PIN change lacks before state

In `PinManager.cpp:346-373`:
```cpp
bool PinManager::setBadgePin(const char* newPin) {
    if (!newPin) return false;
    size_t len = strlen(newPin);
    if (len < BADGE_PIN_MIN || len > BADGE_PIN_MAX) {
        LOG_E(TAG, "Badge PIN must be %d-%d digits", BADGE_PIN_MIN, BADGE_PIN_MAX);
        return false;
    }

    computeBadgeHash(newPin, badgeHash_);
    badgeRetries_ = MAX_RETRIES;

    uint8_t defaultHash[BADGE_HASH_SIZE];
    computeBadgeHash(DEFAULT_BADGE_PIN, defaultHash);
    badgePinIsSet_ = !compareHash(badgeHash_, defaultHash, BADGE_HASH_SIZE);

    saveToStorage();
    LOG_I(TAG, "Badge PIN changed");
    return true;
}
```

**Missing before-state:**
- Previous hash not recorded
- Previous retry count not recorded
- No record of whether this was first-time set or change

### Credential replacement lacks before state

In `fido2_storage.cpp:780-800`:
```cpp
int8_t existing_slot = fido2_storage_find_by_rp_user(rp_id_hash, user_id, user_id_len);
int8_t slot;

if (existing_slot >= 0) {
    // Replace existing credential
    LOG_I("FIDO2", "Replacing existing credential in slot %d", existing_slot);
    slot = existing_slot;

    // Erase existing key and metadata
    erase_slot_data(static_cast<uint8_t>(slot));

    // Update cache: mark as invalid temporarily
    g_storage.creds[slot].valid = false;
    g_storage.cred_count--;
}
```

**Missing before-state:**
- Previous user name not recorded
- Previous sign count not recorded
- Previous curve not recorded

## Recommended Fix

### Step 1: Capture before-state for PIN changes (10 min)

```cpp
bool PinManager::setBadgePin(const char* newPin) {
    // ... validation ...

    // Capture before-state for audit
    uint8_t oldHash[BADGE_HASH_SIZE];
    memcpy(oldHash, badgeHash_, BADGE_HASH_SIZE);
    uint8_t oldRetries = badgeRetries_;

    computeBadgeHash(newPin, badgeHash_);
    badgeRetries_ = MAX_RETRIES;

    // ... rest of function ...

    saveToStorage();

    // Audit with before-state
    core::audit_record(core::AuditEventType::PIN_CHANGE,
                      0, 0, 0, getPinContext(),
                      (oldRetries << 16) | (badgePinIsSet_ ? 1 : 0));

    LOG_I(TAG, "Badge PIN changed");
    return true;
}
```

### Step 2: Capture before-state for credential replacement (10 min)

```cpp
bool fido2_storage_create_credential(...) {
    int8_t existing_slot = fido2_storage_find_by_rp_user(rp_id_hash, user_id, user_id_len);
    int8_t slot;

    if (existing_slot >= 0) {
        // Capture before-state
        fido2_stored_cred_t before;
        read_rmem_credential(existing_slot, &before);

        LOG_I("FIDO2", "Replacing existing credential in slot %d", existing_slot);
        slot = existing_slot;
        erase_slot_data(static_cast<uint8_t>(slot));

        g_storage.creds[slot].valid = false;
        g_storage.cred_count--;

        // Audit replacement with before-state
        core::audit_record(core::AuditEventType::CREDENTIAL_DELETE,
                          1, existing_slot, 0, 0,
                          (before.sign_count << 16) | (before.curve << 8));
    }
    // ...
}
```

## References

- NIST SP 800-53 Rev. 5 AU-14 (Session Audit)
- Common Criteria EAL3+ - FAU_STG.4 (Before/after snapshots)
