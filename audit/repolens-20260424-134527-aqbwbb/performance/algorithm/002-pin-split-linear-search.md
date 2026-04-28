---
title: "[LOW] Linear search for PIN split in change_reference_data"
severity: LOW
domain: algorithm-efficiency
lens: algorithm
labels:
  - "linear-search"
---

## Summary
The `cmd_change_reference_data` function in `components/mod_gpg/src/openpgp/openpgp.cpp` (lines 1123-1211) uses a linear search to find the correct split point between old PIN and new PIN. For PW1, it tries all possible lengths from `OPENPGP_PW1_MIN_LEN` (6) up to the maximum, calling the verification function for each attempt.

**Evidence:**
```cpp
// components/mod_gpg/src/openpgp/openpgp.cpp:1141-1162
// Find split point - try different old PIN lengths
bool changed = false;
for (size_t old_len = OPENPGP_PW1_MIN_LEN; old_len <= apdu->lc - OPENPGP_PW1_MIN_LEN; old_len++) {
    char old_pin[OPENPGP_PIN_MAX_LEN + 1];
    char new_pin[OPENPGP_PIN_MAX_LEN + 1];

    memcpy(old_pin, apdu->data, old_len);
    old_pin[old_len] = '\0';

    size_t new_len = apdu->lc - old_len;
    memcpy(new_pin, apdu->data + old_len, new_len);
    new_pin[new_len] = '\0';

    // Try verifying with this split
    if (pin_storage_openpgp_verify_pw1(old_pin)) {
        if (pin_storage_openpgp_change_pw1(new_pin)) {
            changed = true;
            break;
        }
    }
}
```

Same pattern exists for PW3 (lines 1178-1198).

## Impact
- **Current scale**: With max PIN of 32 chars and min of 6, the loop runs at most ~27 times
- **Cost per iteration**: Each iteration allocates stack buffers, copies data, and performs KDF hash computation
- **Total cost**: Worst case ~27 KDF computations (each with iteration count for security)
- **User experience**: Noticeable delay when changing PINs, especially with high iteration counts

## Recommended Fix
The OpenPGP spec defines fixed minimum lengths (PW1: 6 bytes, PW3: 8 bytes). Instead of trying all splits, use a single-pass approach:

1. **Use standard split**: Assume old PIN is minimum length + verify, then remaining is new PIN
2. **Or use delimiter**: If variable-length PINs are needed, use a known delimiter byte between old and new
3. **Or two-phase**: First verify old PIN (known length), then copy rest as new PIN

Example fix for PW1:
```cpp
// Standard approach: old PIN at minimum length
size_t old_len = OPENPGP_PW1_MIN_LEN;
char old_pin[OPENPGP_PIN_MAX_LEN + 1];
char new_pin[OPENPGP_PIN_MAX_LEN + 1];

memcpy(old_pin, apdu->data, old_len);
old_pin[old_len] = '\0';

size_t new_len = apdu->lc - old_len;
memcpy(new_pin, apdu->data + old_len, new_len);
new_pin[new_len] = '\0';

if (pin_storage_openpgp_verify_pw1(old_pin)) {
    if (pin_storage_openpgp_change_pw1(new_pin)) {
        changed = true;
    }
}
```

## References
- OpenPGP Smart Card Specification 3.4.1, Section 6.5 (CHANGE REFERENCE DATA)
- ISO/IEC 7816-4: Organization, selection and formatting of data
