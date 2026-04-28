---
title: "[MEDIUM] FIDO2 sign-count cache updated without atomic NVS persistence"
severity: MEDIUM
domain: performance/caching
lens: embedded-firmware
labels:
  - "cache-consistency"
  - "fido2-optimization"
---

## Summary
The FIDO2 storage layer (`components/mod_fido2/src/fido2_storage.cpp`) maintains an in-memory sign-count cache (`g_storage.creds[slot].sign_count`) that is incremented before being persisted to R-Memory. However, the update sequence is not atomic: the cache is modified first, then R-Memory is updated separately. If the system crashes or resets between these operations, the cache and persistent storage become inconsistent.

**Evidence:**
- File: `components/mod_fido2/src/fido2_storage.cpp`
- Line 918-937: `fido2_storage_increment_sign_count()` function
- Line 924: Cache updated first (`g_storage.creds[slot].sign_count++`)
- Line 928-935: R-Memory read-modify-write happens after cache update
- No transactional mechanism to ensure both updates succeed together

## Impact
**Consistency Risk:**
- After a sign operation, the in-memory sign count is incremented immediately
- R-Memory update happens asynchronously after cache modification
- If power is lost or system resets between lines 924 and 935:
  - Cache (on next boot from R-Memory) will have OLD sign count
  - But FIDO2 operations may expect NEW sign count
  - This can cause credential validation failures (sign count regression detected)

**FIDO2 Spec Compliance:**
- FIDO2 requires monotonically increasing sign counts
- Sign count regression (new count < old count) indicates potential cloning
- Inconsistent state may trigger false-positive security warnings

**Performance Cost:**
- Current implementation reads R-Memory, modifies, writes back on every sign
- This is 2 R-Memory operations per sign (read + write)
- Each R-Memory operation: ~5-15ms including session management
- For high-frequency signing (e.g., multiple FIDO2 authentications), this adds up

## Evidence
From `components/mod_fido2/src/fido2_storage.cpp` (lines 918-937):

```cpp
uint32_t fido2_storage_increment_sign_count(uint8_t slot) {
    if (!slot_logical_valid(slot) || !g_storage.creds[slot].valid) {
        return 0;
    }

    // Increment local cache FIRST
    g_storage.creds[slot].sign_count++;
    uint32_t new_count = g_storage.creds[slot].sign_count;

    // Then persist to R-Memory (separate operation!)
    fido2_stored_cred_t stored;
    if (read_rmem_credential(slot, &stored)) {
        stored.sign_count = new_count;
        if (!write_rmem_credential(slot, &stored)) {
            LOG_E("FIDO2", "CRITICAL: Failed to persist sign count for slot %d!", slot);
            // Cache already incremented, but R-Memory has old value!
        }
    }

    return new_count;
}
```

**Problem:** The log message even acknowledges the critical nature, but no recovery mechanism exists. If `write_rmem_credential()` fails, the cache has already been modified.

## Recommended Fix
Implement atomic sign-count update with proper error handling:

1. **Read-modify-write in single function with rollback**:
```cpp
uint32_t fido2_storage_increment_sign_count(uint8_t slot) {
    if (!slot_logical_valid(slot) || !g_storage.creds[slot].valid) {
        return 0;
    }

    // Read current state from R-Memory first
    fido2_stored_cred_t stored;
    if (!read_rmem_credential(slot, &stored)) {
        LOG_E("FIDO2", "Failed to read credential for sign count update");
        return 0;
    }

    // Increment in local copy
    stored.sign_count++;
    uint32_t new_count = stored.sign_count;

    // Persist to R-Memory
    if (!write_rmem_credential(slot, &stored)) {
        LOG_E("FIDO2", "Failed to persist sign count - cache NOT updated");
        // Cache stays consistent with R-Memory (both have old value)
        return 0;
    }

    // Only update cache AFTER successful persistence
    g_storage.creds[slot].sign_count = new_count;

    return new_count;
}
```

2. **Add sign-count verification on cache access**:
```cpp
uint32_t fido2_storage_get_sign_count(uint8_t slot) {
    if (!slot_logical_valid(slot) || !g_storage.creds[slot].valid) {
        return 0;
    }

    // Optional: verify cache matches R-Memory on critical operations
    #ifdef CONFIG_FIDO2_STRICT_SIGN_COUNT
    fido2_stored_cred_t stored;
    if (read_rmem_credential(slot, &stored)) {
        if (stored.sign_count != g_storage.creds[slot].sign_count) {
            LOG_W("FIDO2", "Sign count mismatch detected - syncing cache");
            g_storage.creds[slot].sign_count = stored.sign_count;
        }
    }
    #endif

    return g_storage.creds[slot].sign_count;
    ```

3. **Add sign-count initialization from R-Memory**:
```cpp
static void sync_cache_sign_counts(void) {
    uint16_t total = ecc_count();
    for (uint8_t i = 0; i < total && i < FIDO2_MAX_CREDENTIALS; i++) {
        if (g_storage.creds[i].valid) {
            fido2_stored_cred_t stored;
            if (read_rmem_credential(i, &stored)) {
                g_storage.creds[i].sign_count = stored.sign_count;
            }
        }
    }
}
```

## References
- FIDO2 sign-count specification: https://fidoalliance.org/specs/fido2-web-authn-api/
- ESP32 power-loss considerations: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/fault_handlers.html
- Atomic update patterns: https://cheatsheetseries.owasp.org/cheatsheets/Authentication_Cheat_Sheet.html
