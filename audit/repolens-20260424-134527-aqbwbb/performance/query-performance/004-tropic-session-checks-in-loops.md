---
title: "[MEDIUM] TROPIC01 session restart checks on every R-Memory read in credential enumeration loops"
severity: MEDIUM
domain: embedded-storage
lens: query-performance
labels:
  - "session-management"
  - "fido2"
  - "rmem-read"
---

## Summary

In `components/mod_fido2/src/ctap2.cpp`, the credential enumeration functions (`cred_mgmt_count_unique_rps()`, `cred_mgmt_find_creds_for_rp()`) call `fido2_storage_get_credential()` for each slot in a loop. Each call eventually triggers an R-Memory read via `fido2_storage_get_user()`, which goes through `Tropic01Element::rmemRead()`. Every `rmemRead()` call invokes `ensureSession()`, which checks session status and potentially restarts the session if inactive.

**Location**: 
- `components/mod_fido2/src/ctap2.cpp:2976-2996` (cred_mgmt_count_unique_rps)
- `components/mod_fido2/src/ctap2.cpp:3009-3020` (cred_mgmt_find_creds_for_rp)
- `components/mod_fido2/src/fido2_storage.cpp:727-730` (fido2_storage_get_credential → fido2_storage_get_user)
- `components/cdc_hal/src/Tropic01Element.cpp:547-576` (rmemRead → ensureSession)

## Impact

**Performance Cost**:
- For N credentials, N separate `ensureSession()` checks
- Each check involves: lock → session check → potential session restart → unlock
- Session restart involves SHA256-based handshake with TROPIC01 (~5-10ms SPI latency)
- Even when session is active, lock/unlock adds overhead per read

**Code Path**:
```
cred_mgmt_count_unique_rps()
  → for each slot: fido2_storage_get_credential(slot, &info)
    → fido2_storage_get_user(slot, ...)
      → rmemRead(rmem_slot, ...)
        → ensureSession("rmemRead")  // Check + potential restart
        → lt_r_mem_data_read()       // SPI transaction
```

**Scalability**:
- With 30 FIDO2 credentials: 30 session checks, up to 30 SPI transactions
- If session expires mid-enumeration: multiple session restarts possible
- User experience: credential listing can take 100-300ms depending on session state

## Evidence

**Loop in `cred_mgmt_count_unique_rps()` (lines 2976-2996)**:
```cpp
static uint8_t cred_mgmt_count_unique_rps(void) {
    EXT_RAM_BSS_ATTR static uint8_t unique_hashes[FIDO2_MAX_CREDENTIALS][32];
    uint8_t count = 0;

    for (uint8_t slot = 0; slot < FIDO2_MAX_CREDENTIALS; slot++) {
        if (!fido2_storage_is_resident(slot)) continue;

        fido2_credential_info_t info;
        if (!fido2_storage_get_credential(slot, &info)) continue;  // Triggers rmemRead + ensureSession!

        // Check if this RP hash is already in our list
        bool found = false;
        for (uint8_t j = 0; j < count; j++) {
            if (memcmp(unique_hashes[j], info.rp_id_hash, 32) == 0) {
                found = true;
                break;
            }
        }
        // ...
    }
    return count;
}
```

**Session check in `ensureSession()` (Tropic01Element.cpp:277-281)**:
```cpp
bool Tropic01Element::ensureSession(const char* op) {
    if (sessionActive_) return true;  // Fast path if active
    LOG_W(TAG, "Session inactive for %s - restarting", op);
    return sessionStart();  // Slow path: full handshake (~5-10ms)
}
```

**R-Memory read path (Tropic01Element.cpp:547-558)**:
```cpp
SeResult Tropic01Element::rmemRead(uint16_t slot, uint8_t* data, uint16_t maxLen,
                                    uint16_t* actualLen) {
    // ...
    lock();  // Mutex overhead

    if (!ensureSession("rmemRead")) {  // Session check every call
        unlock();
        return SeResult::SESSION_REQUIRED;
    }

    lt_ret_t ret = lt_r_mem_data_read(&handle_, slot, data, maxLen, &bytesRead);
    // ...
    unlock();
    // ...
}
```

**Note**: The cached data `g_storage.creds[slot]` already contains most fields (rp_id, user_name, sign_count, etc.), but `fido2_storage_get_credential()` still calls `fido2_storage_get_user()` to fetch user_id from R-Memory, triggering the full read path.

## Recommended Fix

**Option 1: Add batch enumeration API**

Create a function that enumerates all credentials with a single session check:

```cpp
/**
 * \brief Batch-fetches all credential info with single session check.
 * \param infos Array to receive credential info (max FIDO2_MAX_CREDENTIALS).
 * \param count_out Number of valid credentials found.
 * \return `true` on success.
 */
bool fido2_storage_get_all_credentials(fido2_credential_info_t* infos, uint8_t* count_out) {
    auto* se = get_se();
    if (!se) return false;

    // Single session check for entire batch
    if (!se->isSessionActive()) {
        if (!se->sessionStart()) return false;
    }

    *count_out = 0;
    for (uint8_t slot = 0; slot < FIDO2_MAX_CREDENTIALS && *count_out < FIDO2_MAX_CREDENTIALS; slot++) {
        if (!g_storage.creds[slot].valid) continue;

        // Use cached data (no R-Memory read for most fields)
        memcpy(&infos[*count_out], &g_storage.creds[slot], sizeof(fido2_credential_info_t));
        
        // Only fetch user_id if needed (optional optimization)
        // Or skip it entirely if UI doesn't need user_id
        
        (*count_out)++;
    }
    return true;
}
```

**Option 2: Use cached data directly for enumeration**

Modify `cred_mgmt_count_unique_rps()` and `cred_mgmt_find_creds_for_rp()` to use `g_storage.creds[]` directly:

```cpp
static uint8_t cred_mgmt_count_unique_rps(void) {
    EXT_RAM_BSS_ATTR static uint8_t unique_hashes[FIDO2_MAX_CREDENTIALS][32];
    uint8_t count = 0;

    for (uint8_t slot = 0; slot < FIDO2_MAX_CREDENTIALS; slot++) {
        // Use cached data directly - no R-Memory read!
        if (!g_storage.creds[slot].valid || !g_storage.creds[slot].resident) continue;

        // Check if this RP hash is already in our list
        bool found = false;
        for (uint8_t j = 0; j < count; j++) {
            if (memcmp(unique_hashes[j], g_storage.creds[slot].rp_id_hash, 32) == 0) {
                found = true;
                break;
            }
        }
        // ...
    }
    return count;
}
```

**Option 3: Session-aware batch read**

Add a version of `rmemRead` that assumes session is already active (caller responsibility):

```cpp
/**
 * \brief R-Memory read without session check (caller must ensure session).
 * \param slot R-memory slot index.
 * \param data Destination buffer.
 * \param maxLen Size of `data`.
 * \param actualLen Bytes read.
 * \return Operation result.
 */
SeResult Tropic01Element::rmemReadFast(uint16_t slot, uint8_t* data, uint16_t maxLen, uint16_t* actualLen) {
    // Assumes session is active - no ensureSession() call
    // Caller must lock() before calling
    lt_ret_t ret = lt_r_mem_data_read(&handle_, slot, data, maxLen, actualLen);
    return mapResult(ret);
}

// Usage in batch context:
void enumerateCredentials() {
    se->lock();
    se->ensureSession("enumerate");  // Single check
    for (auto slot : slots) {
        se->rmemReadFast(slot, ...);  // No repeated checks
    }
    se->unlock();
}
```

**Recommended approach**: Option 2 is simplest (~30-45 minutes). The `g_storage.creds[]` cache already has all fields needed for enumeration (rp_id_hash, rp_id, user_name, resident flag). The user_id is only needed for specific operations, not for listing.

## References

- [TROPIC01 Session Management](https://www.microchip.com/en-us/products/security-ics/secure-elements/tropic01) - Session handshake involves SHA256 computation and multiple SPI transactions
- [libtropic API](components/cdc_hal/src/Tropic01Element.cpp) - `ensureSession()` checks `sessionActive_` flag and calls `sessionStart()` on miss
- [FIDO2 Credential Metadata](components/mod_fido2/src/fido2_storage.cpp) - `g_storage.creds[]` populated during `fido2_storage_init()` from R-Memory

(End of file - total 174 lines)
