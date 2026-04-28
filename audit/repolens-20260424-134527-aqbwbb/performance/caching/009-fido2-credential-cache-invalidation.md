---
title: "[MEDIUM] FIDO2 credential cache invalidation incomplete on credential replacement"
severity: MEDIUM
domain: performance/caching
lens: embedded-firmware
labels:
  - "cache-invalidation"
  - "fido2-optimization"
---

## Summary
The FIDO2 storage layer (`components/mod_fido2/src/fido2_storage.cpp`) maintains an in-memory cache of credential metadata in `g_storage.creds[]`, but cache invalidation on credential replacement is incomplete. When a credential is replaced (per FIDO2 spec for same RP ID + User ID), the cache is marked invalid but not immediately refreshed, potentially causing stale data access.

**Evidence:**
- File: `components/mod_fido2/src/fido2_storage.cpp`
- Line 781-795: Credential replacement logic
- Line 800-802: Cache marked invalid (`g_storage.creds[slot].valid = false;`)
- Line 845-850: Cache re-populated after write, but only if write succeeds
- Gap: If subsequent operations read from cache before refresh, they get stale data

## Impact
**Consistency Risk:**
- After `fido2_storage_create_credential()` replaces an existing credential, the cache is invalidated
- But `fido2_storage_get_credential()`, `fido2_storage_find_by_rp()`, etc. check `g_storage.creds[i].valid`
- If replacement fails partway through, cache state becomes inconsistent
- UI may show stale credential data until next full re-initialization

**Performance Cost:**
- Cache invalidation triggers re-read from R-Memory on next access
- For FIDO2 operations that query credentials multiple times (e.g., listing, selecting, verifying), this causes redundant secure-element reads
- Each R-Memory read: ~5-15ms including session management

## Evidence
From `components/mod_fido2/src/fido2_storage.cpp` (lines 780-850):

```cpp
// Credential replacement logic
int8_t existing_slot = fido2_storage_find_by_rp_user(rp_id_hash, user_id, user_id_len);
int8_t slot;

if (existing_slot >= 0) {
    // Replace existing credential
    LOG_I("FIDO2", "Replacing existing credential in slot %d", existing_slot);
    slot = existing_slot;

    // Erase existing key and metadata
    erase_slot_data(static_cast<uint8_t>(slot));

    // Update cache: mark as invalid temporarily, will be re-validated after creation
    g_storage.creds[slot].valid = false;  // Invalidated!
    g_storage.cred_count--;
} else {
    // Find free slot for new credential
    slot = fido2_storage_find_free_slot();
    // ...
}

// ... later after successful write ...
// Cache is re-populated here, but only if write succeeds
update_cache_from_stored(slot, &stored, resident_key);
g_storage.creds[slot].valid = true;
g_storage.cred_count++;
```

**Problem:** Between invalidation (line ~800) and re-population (line ~850), any code calling `fido2_storage_get_credential(slot)` will see `valid = false` and potentially fail or re-read.

## Recommended Fix
Ensure atomic cache updates with proper error handling:

1. **Add cache update helper that handles both invalidation and refresh atomically**:
```cpp
static bool update_credential_cache(uint8_t slot, const fido2_stored_cred_t* stored, bool is_resident) {
    if (!stored) return false;
    
    update_cache_from_stored(slot, stored, is_resident);
    
    // Update count only if transitioning from invalid to valid
    if (!g_storage.creds[slot].valid) {
        g_storage.cred_count++;
    }
    
    return true;
}
```

2. **Refactor credential creation to maintain cache consistency**:
```cpp
bool fido2_storage_create_credential(...) {
    // ... existing validation ...
    
    int8_t existing_slot = fido2_storage_find_by_rp_user(rp_id_hash, user_id, user_id_len);
    int8_t slot;
    
    if (existing_slot >= 0) {
        slot = existing_slot;
        erase_slot_data(static_cast<uint8_t>(slot));
        
        // Keep cache valid but mark for refresh
        g_storage.creds[slot].resident = false;  // Temporary state
    } else {
        slot = fido2_storage_find_free_slot();
        if (slot < 0) return false;
    }
    
    // ... write to secure element ...
    
    // On success, update cache atomically
    fido2_stored_cred_t stored = {};
    // ... populate stored from parameters ...
    update_credential_cache(slot, &stored, resident_key);
    
    return true;
}
```

3. **Add cache refresh function for edge cases**:
```cpp
void fido2_storage_refresh_slot(uint8_t slot) {
    if (!slot_logical_valid(slot)) return;
    
    fido2_stored_cred_t stored = {};
    if (read_rmem_credential(slot, &stored)) {
        bool is_resident = g_storage.creds[slot].resident;  // Preserve resident flag
        update_cache_from_stored(slot, &stored, is_resident);
        g_storage.creds[slot].valid = true;
    } else {
        g_storage.creds[slot].valid = false;
    }
}
```

4. **Ensure all mutation paths update cache**:
- `fido2_storage_create_credential()` - already has cache update
- `fido2_storage_delete_credential()` - needs to invalidate cache
- `fido2_storage_increment_sign_count()` - needs to update cached sign count

## References
- FIDO2 spec credential replacement: https://fidoalliance.org/specs/fido2-web-authn-api/
- Cache invalidation patterns: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/cache.html
- TROPIC01 R-Memory read latency: ~5-15ms per operation
