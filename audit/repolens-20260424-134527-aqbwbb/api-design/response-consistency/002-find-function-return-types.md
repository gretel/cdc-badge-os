---
title: "[MEDIUM] Inconsistent return types for similar find functions in FIDO2 storage"
severity: MEDIUM
domain: api-design
lens: response-consistency
labels:
  - "audit:api-design/response-consistency"
  - "api-consistency"
  - "mod_fido2"
---

## Summary

The FIDO2 storage module uses inconsistent return types for similar "find" functions, making the API confusing and error-prone.

**Location:**
- `components/mod_fido2/src/fido2_storage.cpp` - Lines 490-620 (find functions)

**Return type inconsistency:**

| Function | Return Type | Success | Failure | Meaning |
|----------|-------------|---------|---------|---------|
| `fido2_storage_find_free_slot()` | `int8_t` | slot index (0-31) | `-1` | Single slot |
| `fido2_storage_find_by_rp(...)` | `uint8_t` | count (0-N) | `0` | Multiple slots |
| `fido2_storage_find_by_rp_resident(...)` | `uint8_t` | count (0-N) | `0` | Multiple slots |
| `fido2_storage_find_by_rp_user(...)` | `int8_t` | slot index (0-31) | `-1` | Single slot |
| `fido2_storage_find_slot_by_cred_id(...)` | `int8_t` | slot index (0-31) | `-1` | Single slot |

**Inconsistencies:**
1. **`find_by_rp` returns `uint8_t`** - count of slots found (0 means none)
2. **`find_by_rp_user` returns `int8_t`** - single slot index (-1 means none)
3. Both functions search by RP ID, but return different types

## Impact

**API confusion:** Developers must remember which function returns count vs. slot index.

**Error-prone usage:**
```cpp
// find_by_rp returns 0 when no slots found (could be valid count)
uint8_t count = fido2_storage_find_by_rp(hash, slots, 8);
if (count == 0) {  // "0 slots found" - is this an error?
    // Handle "not found"
}

// find_by_rp_user returns -1 when no slot found (clearer)
int8_t slot = fido2_storage_find_by_rp_user(hash, user_id, 8);
if (slot == -1) {  // "not found" - unambiguous
    // Handle "not found"
}
```

**Inconsistent error handling:** Code using these functions must handle both `0` and `-1` as "not found".

## Evidence

**Function signatures (lines 490-620):**

```cpp
// Line 490: Returns int8_t with -1 for not found
int8_t fido2_storage_find_free_slot(void) {
    uint16_t count = ecc_count();
    for (uint8_t i = 0; i < count && i < FIDO2_MAX_CREDENTIALS; i++) {
        if (!g_storage.creds[i].valid) {
            return i;
        }
    }
    return -1;  // Not found
}

// Line 507: Returns uint8_t count (0 means none found)
uint8_t fido2_storage_find_by_rp(const uint8_t *rp_id_hash,
                                  uint8_t *out_slots, uint8_t max_slots) {
    uint8_t count = 0;
    uint16_t total = ecc_count();
    for (uint8_t i = 0; i < total && i < FIDO2_MAX_CREDENTIALS && count < max_slots; i++) {
        if (g_storage.creds[i].valid &&
            memcmp(g_storage.creds[i].rp_id_hash, rp_id_hash, 32) == 0) {
            out_slots[count++] = i;
        }
    }
    return count;  // 0 means none found
}

// Line 529: Returns uint8_t count (0 means none found)
uint8_t fido2_storage_find_by_rp_resident(const uint8_t *rp_id_hash,
                                          uint8_t *out_slots, uint8_t max_slots) {
    uint8_t count = 0;
    // ... similar to find_by_rp
    return count;
}

// Line 567: Returns int8_t with -1 for not found
int8_t fido2_storage_find_by_rp_user(const uint8_t *rp_id_hash,
                                      const uint8_t *user_id, uint8_t user_id_len) {
    uint16_t total = ecc_count();
    for (uint8_t i = 0; i < total && i < FIDO2_MAX_CREDENTIALS; i++) {
        if (g_storage.creds[i].valid &&
            memcmp(g_storage.creds[i].rp_id_hash, rp_id_hash, 32) == 0 &&
            memcmp(g_storage.creds[i].user_id, user_id, user_id_len) == 0) {
            return i;
        }
    }
    return -1;  // Not found
}

// Line 601: Returns int8_t with -1 for not found
int8_t fido2_storage_find_slot_by_cred_id(const uint8_t *cred_id, uint16_t cred_id_len) {
    if (!cred_id || cred_id_len != FIDO2_CRED_ID_LEN) return -1;
    uint8_t slot = cred_id[0];
    if (!slot_logical_valid(slot) || !g_storage.creds[slot].valid) {
        return -1;
    }
    // ... validation ...
    return (fido2_storage_get_cred_id(slot, stored_id)) ? slot : -1;
}
```

**Usage in ctap2.cpp (line 781):**
```cpp
int8_t existing_slot = fido2_storage_find_by_rp_user(rp_id_hash, user_id, user_id_len);
if (existing_slot >= 0) {
    // Replace existing credential
}
```

## Recommended Fix

### Option 1: Standardize on `int8_t` with -1 for not found (Recommended)

Change `find_by_rp` and `find_by_rp_resident` to return single slot or -1:

```cpp
// For find_by_rp - return first match or -1
int8_t fido2_storage_find_by_rp(const uint8_t *rp_id_hash) {
    uint16_t total = ecc_count();
    for (uint8_t i = 0; i < total && i < FIDO2_MAX_CREDENTIALS; i++) {
        if (g_storage.creds[i].valid &&
            memcmp(g_storage.creds[i].rp_id_hash, rp_id_hash, 32) == 0) {
            return i;  // First match
        }
    }
    return -1;  // Not found
}

// For multiple results, use array output parameter
uint8_t fido2_storage_find_all_by_rp(const uint8_t *rp_id_hash, 
                                      uint8_t *out_slots, uint8_t max_slots) {
    uint8_t count = 0;
    uint16_t total = ecc_count();
    for (uint8_t i = 0; i < total && i < FIDO2_MAX_CREDENTIALS && count < max_slots; i++) {
        if (g_storage.creds[i].valid &&
            memcmp(g_storage.creds[i].rp_id_hash, rp_id_hash, 32) == 0) {
            out_slots[count++] = i;
        }
    }
    return count;
}
```

### Option 2: Use enum for status + output parameter

```cpp
typedef enum {
    FIND_OK,
    FIND_NOT_FOUND,
    FIND_INVALID_PARAM,
} FindResult;

FindResult fido2_storage_find_by_rp(const uint8_t *rp_id_hash, uint8_t *out_slot);
```

### Update callers:

```cpp
// Before (inconsistent)
int8_t existing_slot = fido2_storage_find_by_rp_user(...);
if (existing_slot >= 0) { ... }

uint8_t count = fido2_storage_find_by_rp(...);
if (count > 0) { ... }

// After (consistent)
int8_t slot = fido2_storage_find_by_rp(...);
if (slot >= 0) { ... }  // Same pattern for all find functions
```

## References

- Similar issue: `007-boolean-error-masking.md` (error code consistency)
- C API conventions: Use `int` with `-1` for failure (e.g., `strchr`, `strstr`)
- ESP-IDF conventions: `esp_err_t` returns specific error codes
