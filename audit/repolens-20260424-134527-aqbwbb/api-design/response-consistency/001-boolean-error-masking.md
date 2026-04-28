---
title: "[HIGH] Boolean return type masks specific error codes in fido2_storage_create_credential"
severity: HIGH
domain: api-design
lens: response-consistency
labels:
  - "audit:api-design/response-consistency"
  - "error-handling"
  - "mod_fido2"
---

## Summary

The `fido2_storage_create_credential` function returns a generic `bool` (true/false) instead of specific error codes, causing callers to lose error context and return incorrect CTAP2 status codes.

**Locations:**
- `components/mod_fido2/src/fido2_storage.cpp` - Lines 762-888 (function definition)
- `components/mod_fido2/src/ctap2.cpp` - Lines 1088-1092 (caller)

**Failure modes all returning `false`:**

| Line | Failure Condition | Actual Error | Mapped Error |
|------|-------------------|--------------|--------------|
| 777 | User ID too long | Invalid parameter | `CTAP2_ERR_KEY_STORE_FULL` |
| 800 | No free ECC slots | Key store full | `CTAP2_ERR_KEY_STORE_FULL` ✓ |
| 812 | Secure element unavailable | Hardware error | `CTAP2_ERR_KEY_STORE_FULL` |
| 821 | ECC key generation failed | Hardware/processing error | `CTAP2_ERR_KEY_STORE_FULL` |
| 833 | Public key read failed | Hardware error | `CTAP2_ERR_KEY_STORE_FULL` |
| 841 | Random number generation failed | Hardware error | `CTAP2_ERR_KEY_STORE_full` |
| 875 | R-Memory write failed | Storage error | `CTAP2_ERR_KEY_STORE_FULL` |

## Impact

**Incorrect error reporting:** Clients receive `CTAP2_ERR_KEY_STORE_FULL` for errors that are NOT about the key store being full:
- User ID too long → Should be `CTAP2_ERR_INVALID_CBOR` or `CTAP2_ERR_processing`
- Secure element unavailable → Should be `CTAP2_ERR_OTHER`
- Key generation failed → Should be `CTAP2_ERR_OTHER`
- R-Memory write failed → Should be `CTAP2_ERR_OTHER` or `CTAP2_ERR_KEY_STORE_FULL`

**Poor debugging experience:** Users/parsers cannot distinguish between:
- "Try again later" (key store full)
- "Fix your request" (invalid user ID)
- "Hardware problem" (secure element error)

**CTAP2 spec compliance:** The CTAP2 specification defines specific error codes for different failure conditions. Using the wrong code violates the spec and may cause interoperability issues.

## Evidence

**Caller in ctap2.cpp (lines 1088-1092):**
```cpp
LOG_I("CTAP2", "Calling fido2_storage_create_credential...");
if (!fido2_storage_create_credential(
        p->rp_id, p->rp_id_hash, p->user_id, p->user_id_len, p->user_name,
        p->rk, p->cred_protect, curve, &slot, cred_id, pubkey)) {
    response[0] = CTAP2_ERR_KEY_STORE_FULL;  // Always returns KEY_STORE_FULL!
    *response_len = 1;
    return CTAP2_ERR_KEY_STORE_FULL;
}
```

**Failure points in fido2_storage.cpp:**

```cpp
// Line 775-778: User ID validation
if (user_id && user_id_len > FIDO2_USER_ID_MAX_LEN) {
    LOG_E("FIDO2", "User ID too long: %u", user_id_len);
    return false;  // Should be CTAP2_ERR_INVALID_CBOR
}

// Line 798-801: No free slots
slot = fido2_storage_find_free_slot();
if (slot < 0) {
    LOG_E("FIDO2", "No free slots");
    return false;  // Correct: CTAP2_ERR_KEY_STORE_FULL
}

// Line 812-813: Secure element unavailable
auto* se = get_se();
if (!se) return false;  // Should be CTAP2_ERR_OTHER

// Line 819-822: Key generation failed
if (se->eccGenerate(phys_slot, se_curve) != cdc::hal::SeResult::OK) {
    LOG_E("FIDO2", "Failed to generate %s key in slot %d", curve_name, slot);
    return false;  // Should be CTAP2_ERR_OTHER
}

// Line 830-834: Public key read failed
if (se->eccGetPublicKey(phys_slot, pubkey, &se_read_curve) != cdc::hal::SeResult::OK) {
    LOG_E("FIDO2", "Failed to read public key from slot %d", slot);
    se->eccDelete(phys_slot);
    return false;  // Should be CTAP2_ERR_OTHER
}

// Line 873-876: R-Memory write failed
if (!write_rmem_credential(static_cast<uint8_t>(slot), &stored)) {
    se->eccDelete(phys_slot);
    return false;  // Should be CTAP2_ERR_OTHER or KEY_STORE_FULL
}
```

## Recommended Fix

### Option 1: Return CTAP2 error codes directly (Recommended)

Change the function signature to return `uint8_t` (CTAP2 status code):

```cpp
// In fido2_storage.h
uint8_t fido2_storage_create_credential(
    const char *rp_id,
    const uint8_t *rp_id_hash,
    const uint8_t *user_id,
    uint8_t user_id_len,
    const char *user_name,
    bool resident_key,
    uint8_t cred_protect,
    uint8_t curve,
    uint8_t *out_slot,
    uint8_t *out_cred_id,
    uint8_t *out_pubkey
);

// In fido2_storage.cpp
uint8_t fido2_storage_create_credential(...) {
    if (user_id && user_id_len > FIDO2_USER_ID_MAX_LEN) {
        LOG_E("FIDO2", "User ID too long: %u", user_id_len);
        return CTAP2_ERR_INVALID_CBOR;
    }

    int8_t existing_slot = fido2_storage_find_by_rp_user(rp_id_hash, user_id, user_id_len);
    int8_t slot;

    if (existing_slot >= 0) {
        // Replace existing credential
        LOG_I("FIDO2", "Replacing existing credential in slot %d", existing_slot);
        slot = existing_slot;
        erase_slot_data(static_cast<uint8_t>(slot));
        g_storage.creds[slot].valid = false;
        g_storage.cred_count--;
    } else {
        slot = fido2_storage_find_free_slot();
        if (slot < 0) {
            LOG_E("FIDO2", "No free slots");
            return CTAP2_ERR_KEY_STORE_FULL;
        }
    }

    auto* se = get_se();
    if (!se) return CTAP2_ERR_OTHER;  // Changed

    // ... rest of function ...

    if (se->eccGenerate(phys_slot, se_curve) != cdc::hal::SeResult::OK) {
        LOG_E("FIDO2", "Failed to generate %s key in slot %d", curve_name, slot);
        return CTAP2_ERR_OTHER;  // Changed
    }

    if (se->eccGetPublicKey(phys_slot, pubkey, &se_read_curve) != cdc::hal::SeResult::OK) {
        LOG_E("FIDO2", "Failed to read public key from slot %d", slot);
        se->eccDelete(phys_slot);
        return CTAP2_ERR_OTHER;  // Changed
    }

    if (!write_rmem_credential(static_cast<uint8_t>(slot), &stored)) {
        se->eccDelete(phys_slot);
        return CTAP2_ERR_OTHER;  // Changed
    }

    // ... success ...
    return CTAP2_OK;
}
```

### Option 2: Use enum with error detail

```cpp
typedef enum {
    CREDCREATE_OK = 0,
    CREDCREATE_INVALID_PARAM,
    CREDCREATE_NO_SLOTS,
    CREDCREATE_SE_NOT_AVAILABLE,
    CREDCREATE_KEY_GEN_FAILED,
    CREDCREATE_KEY_READ_FAILED,
    CREDCREATE_RNG_FAILED,
    CREDCREATE_RMEM_WRITE_FAILED,
} CredCreateResult;

CredCreateResult fido2_storage_create_credential(...);
```

Then map to CTAP2 codes in the caller.

### Update caller in ctap2.cpp:

```cpp
uint8_t status = fido2_storage_create_credential(
    p->rp_id, p->rp_id_hash, p->user_id, p->user_id_len, p->user_name,
    p->rk, p->cred_protect, curve, &slot, cred_id, pubkey);

if (status != CTAP2_OK) {
    response[0] = status;
    *response_len = 1;
    return status;
}
```

## References

- [CTAP2 Error Codes Specification](https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-v2.0-ps-20190130.html#error-handling)
- Similar issue pattern in existing findings: `002-inconsistent-error-format.md`
- ESP-IDF error handling: `esp_err_t` returns specific codes, not booleans
