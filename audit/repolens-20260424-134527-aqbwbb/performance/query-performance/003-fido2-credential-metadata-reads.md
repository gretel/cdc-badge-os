---
title: "[LOW] FIDO2 credential listing reads R-Memory for each credential in loop"
severity: LOW
domain: embedded-storage
lens: query-performance
labels:
  - "rmem-read"
  - "fido2"
  - "credential-list"
---

## Summary

In `components/mod_fido2/src/Fido2Ui.cpp`, the `rebuildList()` function calls `fido2_get_credential_info()` for each credential slot in a loop. Each call triggers a read from TROPIC01 R-Memory (~180 bytes per credential) to fetch user data.

**Location**: `components/mod_fido2/src/Fido2Ui.cpp:138-155`

## Impact

**Performance Cost**:
- For N credentials, N separate R-Memory reads
- Each R-Memory read involves SPI communication with TROPIC01
- Typical secure element SPI latency: ~100-500us per read
- For 10 credentials: ~1-5ms total; for 30 credentials: ~3-15ms

**Memory Bandwidth**:
- Each read fetches ~180 bytes (full `fido2_stored_cred_t` structure)
- Only a subset of fields (user_name, rp_id, curve) are actually needed for display

**Scalability**:
- Display rebuild time scales linearly with credential count
- Becomes noticeable when user has many credentials (>20)

## Evidence

**Loop in `rebuildList()` (lines 138-155)**:
```cpp
for (uint8_t i = 0; i < count && i < FIDO2_MAX_CREDENTIALS; i++) {
    s_sortMap[i] = i;
    fido2_credential_info_t info = {};
    if (fido2_get_credential_info(i, &info)) {  // Triggers R-Memory read!
        if (strlen(info.user_name) > 0) {
            snprintf(s_labels[i], sizeof(s_labels[i]),
                     "%.45s (%.45s)", info.rp_id, info.user_name);
        } else {
            snprintf(s_labels[i], sizeof(s_labels[i]),
                     "%.45s", info.rp_id);
        }
    }
}
```

**fido2_get_credential_info() implementation** (`fido2_storage.cpp:708-735`):
```cpp
bool fido2_storage_get_credential(uint8_t slot, fido2_credential_info_t *info) {
    // ...
    // Load user ID from R-Memory (not cached)
    uint8_t user_id_len = 0;
    if (fido2_storage_get_user(slot, info->user_id, &user_id_len,
                               info->user_name, FIDO2_USER_NAME_MAX_LEN)) {  // R-Memory read!
        info->user_id_len = user_id_len;
    }
    return true;
}
```

**Note**: The basic metadata (rp_id, user_name, sign_count) IS cached in `g_storage.creds[]` by `fido2_storage_init()`. However, `fido2_get_credential_info()` still calls `fido2_storage_get_user()` which reads R-Memory again.

## Recommended Fix

**Use cached data directly for display**:

Modify `rebuildList()` to use the cached `g_storage` structure directly instead of calling `fido2_get_credential_info()`:

```cpp
static void rebuildList() {
    s_listCount = 0;
    uint8_t count = fido2_get_credential_count();
    if (count == 0) {
        // ... existing placeholder code ...
        return;
    }

    for (uint8_t i = 0; i < count && i < FIDO2_MAX_CREDENTIALS; i++) {
        s_sortMap[i] = i;
        // Use cached data directly - no R-Memory read!
        if (g_storage.creds[i].valid) {
            if (strlen(g_storage.creds[i].user_name) > 0) {
                snprintf(s_labels[i], sizeof(s_labels[i]),
                         "%.45s (%.45s)",
                         g_storage.creds[i].rp_id,
                         g_storage.creds[i].user_name);
            } else {
                snprintf(s_labels[i], sizeof(s_labels[i]),
                         "%.45s", g_storage.creds[i].rp_id);
            }
        }
    }
    // ... rest unchanged ...
}
```

**Alternative**: Add a "fast path" version of `fido2_get_credential_info()` that uses cached data only:

```cpp
bool fido2_get_credential_info_cached(uint8_t slot, fido2_credential_info_t *info) {
    // Uses only g_storage.creds[] - zero R-Memory reads
    // Slightly less complete (user_id from R-Memory not included)
    // But sufficient for UI display purposes
}
```

**Implementation steps** (~30-45 minutes):
1. Change `rebuildList()` to access `g_storage.creds[]` directly
2. Verify that all fields needed for display are in the cache
3. Test with multiple credentials to confirm no R-Memory reads during list rebuild

## References

- [TROPIC01 R-Memory](https://www.microchip.com/en-us/products/security-ics/secure-elements/tropic01) - R-Memory is accessed via SPI with typical 100-500us latency
- [FIDO2 Credential Storage](components/mod_fido2/src/fido2_storage.cpp) - Cached in `g_storage.creds[]` during `fido2_storage_init()`
