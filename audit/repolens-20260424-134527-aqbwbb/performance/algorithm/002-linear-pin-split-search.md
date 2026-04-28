---
title: "[LOW] Linear PIN split search in cmd_change_reference_data() - O(n) string operations per change attempt"
severity: LOW
domain: mod_gpg
lens: algorithm-efficiency
labels:
  - "redundant-computation"
  - "string-operations"
---

## Summary
In `components/mod_gpg/src/openpgp/openpgp.cpp`, the `cmd_change_reference_data()` function (lines 1123-1211) uses a **linear search loop** to find the correct split point between old and new PIN. For each iteration, it performs:
1. `memcpy()` to extract old PIN
2. `memcpy()` to extract new PIN  
3. String null-termination
4. `pin_storage_openpgp_verify_pw1()` (or PW3)
5. `pin_storage_openpgp_change_pw1()` (or PW3)

With a typical APDU data length of 16-32 bytes and minimum PIN length of 6 bytes, this results in **4-20 loop iterations** with **2 memcpy operations and 2 function calls per iteration**.

## Impact
- **Performance**: 8-40 memory operations per PIN change, plus 4-20 verification attempts
- **Verification overhead**: Up to 20 calls to `pin_storage_openpgp_verify_pw1()` even if the first split is correct
- **Security side-channel**: Variable time based on PIN length (timing attack vulnerability)
- **Code complexity**: Nested loops with string manipulation increase bug surface

## Evidence
**File: `components/mod_gpg/src/openpgp/openpgp.cpp`**

Lines 1141-1162:
```cpp
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
            ESP_LOGI(TAG, "PW1 changed successfully");
            changed = true;
            break;
        }
    }
}
```

Same pattern repeated for PW3 at lines 1180-1198.

## Recommended Fix
**Option 1: Use fixed-length PIN format**
If the protocol allows, specify that PINs are fixed-length (e.g., always 6 digits for PW1, 8 for PW3):
```cpp
if (pw_ref == 0x81) {
    // PW1: fixed 6-byte old + 6-byte new = 12 bytes
    if (apdu->lc != OPENPGP_PW1_MIN_LEN * 2) {
        return apdu_sw(resp, SW_WRONG_LENGTH);
    }
    char old_pin[OPENPGP_PIN_MAX_LEN + 1];
    char new_pin[OPENPGP_PIN_MAX_LEN + 1];
    memcpy(old_pin, apdu->data, 6);
    old_pin[6] = '\0';
    memcpy(new_pin, apdu->data + 6, 6);
    new_pin[6] = '\0';
    
    if (pin_storage_openpgp_verify_pw1(old_pin) &&
        pin_storage_openpgp_change_pw1(new_pin)) {
        return apdu_sw(resp, SW_OK);
    }
    // ...
}
```

**Option 2: Pre-compute split using length prefix**
Include explicit length byte(s) in the data format:
```cpp
// Data format: [old_len][old_pin][new_len][new_pin]
if (pw_ref == 0x81) {
    size_t pos = 0;
    uint8_t old_len = apdu->data[pos++];
    uint8_t new_len = apdu->data[pos + old_len + 1];  // skip old PIN
    
    char old_pin[OPENPGP_PIN_MAX_LEN + 1];
    char new_pin[OPENPGP_PIN_MAX_LEN + 1];
    memcpy(old_pin, apdu->data + pos, old_len);
    old_pin[old_len] = '\0';
    memcpy(new_pin, apdu->data + pos + old_len + 1, new_len);
    new_pin[new_len] = '\0';
    
    // Single verification attempt
    if (pin_storage_openpgp_verify_pw1(old_pin) &&
        pin_storage_openpgp_change_pw1(new_pin)) {
        return apdu_sw(resp, SW_OK);
    }
    // ...
}
```

**Option 3: Early exit with better loop structure**
If variable-length is required, at least minimize string operations:
```cpp
for (size_t old_len = OPENPGP_PW1_MIN_LEN; old_len <= apdu->lc - OPENPGP_PW1_MIN_LEN; old_len++) {
    // Verify first without copying (if pin_storage_openpgp_verify_pw1 accepts pointer + length)
    if (pin_storage_openpgp_verify_pw1_len(apdu->data, old_len)) {
        // Only now copy and change
        char new_pin[OPENPGP_PIN_MAX_LEN + 1];
        size_t new_len = apdu->lc - old_len;
        memcpy(new_pin, apdu->data + old_len, new_len);
        new_pin[new_len] = '\0';
        
        if (pin_storage_openpgp_change_pw1(new_pin)) {
            changed = true;
            break;
        }
    }
}
```

## References
- **Time Complexity**: O(n) iterations with O(1) string ops each → O(n) total
- **Pattern**: "Linear search with expensive body" - reduce body cost or use direct lookup
- **Security**: Variable-time PIN comparison can leak information via timing

---

</content>