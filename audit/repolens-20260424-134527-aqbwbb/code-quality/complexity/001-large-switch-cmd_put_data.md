---
title: "[HIGH] Large switch statement in `cmd_put_data` function with 15+ branches"
severity: HIGH
domain: mod_gpg/openpgp
lens: cyclomatic-complexity
labels:
  - "audit:code-quality/complexity"
---

## Summary

The `cmd_put_data` function in `components/mod_gpg/src/openpgp/openpgp.cpp` (lines 897-1037) contains a large switch statement with 15+ case branches handling different data objects. The function spans 140 lines with repetitive patterns that could be refactored.

**Location:** `components/mod_gpg/src/openpgp/openpgp.cpp:897-1037`

## Impact

- **Maintainability:** Adding new data objects requires understanding 140 lines of similar code
- **Testing:** Each branch needs independent test coverage
- **Error-prone:** Copy-paste pattern increases risk of subtle bugs when adding new cases
- **Readability:** The repetitive structure makes it hard to see the "forest for the trees"

## Evidence

The function contains these switch branches:
```cpp
switch (tag) {
    case 0x005B: // Cardholder name
    case 0x5F2D: // Language preference
    case 0x5F35: // Sex
    case DO_FP_SIG: // Fingerprint SIG
    case DO_FP_DEC: // Fingerprint DEC
    case DO_FP_AUT: // Fingerprint AUT
    case DO_CA_FP_1: // CA Fingerprint 1
    case DO_CA_FP_2: // CA Fingerprint 2
    case DO_CA_FP_3: // CA Fingerprint 3
    case DO_GEN_TIME_SIG: // Generation time SIG
    case DO_GEN_TIME_DEC: // Generation time DEC
    case DO_GEN_TIME_AUT: // Generation time AUT
    case DO_URL: // URL
    case DO_LOGIN: // Login data
    default:
}
```

Many branches follow the identical pattern:
```cpp
case DO_FP_SIG:
    if (apdu->lc == 20) {
        memcpy(fingerprint_sig, apdu->data, 20);
        save_state_to_nvs();
        ESP_LOGI(TAG, "Fingerprint SIG stored");
        return apdu_sw(resp, SW_OK);
    }
    return apdu_sw(resp, SW_WRONG_LENGTH);
```

## Recommended Fix

1. **Create a data object descriptor table** with struct containing:
   - Tag value
   - Pointer to target buffer
   - Buffer size
   - Optional name for logging

2. **Implement a lookup function** that finds the descriptor by tag

3. **Replace switch with generic handler**:
```cpp
static const DataObjectDesc* findDataObject(uint16_t tag);

static int cmd_put_data(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    if (!pw3_verified) {
        return apdu_sw(resp, SW_SECURITY_NOT_SATISFIED);
    }

    const DataObjectDesc* desc = findDataObject((apdu->p1 << 8) | apdu->p2);
    if (!desc) {
        return apdu_sw(resp, SW_FILE_NOT_FOUND);
    }

    if (apdu->lc <= desc->maxSize) {
        memcpy(desc->buffer, apdu->data, apdu->lc);
        save_state_to_nvs();
        return apdu_sw(resp, SW_OK);
    }
    return apdu_sw(resp, SW_WRONG_LENGTH);
}
```

4. **Keep special cases** (like cardholder name with null-termination) as separate handlers if needed

**Estimated effort:** ~1 hour to refactor

## References

- Cyclomatic Complexity: https://en.wikipedia.org/wiki/Cyclomatic_complexity
- Switch statement refactoring: https://refactoring.com/catalog/replaceConditionalWithPolymorphism.html
- Table-driven methods: https://en.wikipedia.org/wiki/Type_table
