---
title: "[MEDIUM] Deep nesting in `cmd_change_reference_data` with for-loop inside if-else"
severity: MEDIUM
domain: mod_gpg/openpgp
lens: cyclomatic-complexity
labels:
  - "audit:code-quality/complexity"
---

## Summary

The `cmd_change_reference_data` function in `components/mod_gpg/src/openpgp/openpgp.cpp` (lines 1123-1211) contains nested loops and conditionals creating a "pyramid of doom" structure. The function has 3 levels of nesting (if → for → if-else) with complex retry logic.

**Location:** `components/mod_gpg/src/openpgp/openpgp.cpp:1123-1211`

## Impact

- **Readability:** Deep nesting makes the control flow hard to follow
- **Testing:** Multiple nested branches require combinatorial test coverage
- **Modification risk:** Adding new logic requires understanding nested context
- **Cyclomatic complexity:** Estimated 10+ decision paths in a 90-line function

## Evidence

```cpp
static int cmd_change_reference_data(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    uint8_t pw_ref = apdu->p2;

    if (apdu->lc == 0) {  // Level 1
        return apdu_sw(resp, SW_WRONG_LENGTH);
    }

    if (pw_ref == 0x81) {  // Level 1
        // Change PW1 (User PIN)
        if (apdu->lc < OPENPGP_PW1_MIN_LEN * 2) {  // Level 2
            return apdu_sw(resp, SW_WRONG_LENGTH);
        }

        bool changed = false;
        for (size_t old_len = OPENPGP_PW1_MIN_LEN; old_len <= apdu->lc - OPENPGP_PW1_MIN_LEN; old_len++) {  // Level 2
            char old_pin[OPENPGP_PIN_MAX_LEN + 1];
            char new_pin[OPENPGP_PIN_MAX_LEN + 1];

            memcpy(old_pin, apdu->data, old_len);
            old_pin[old_len] = '\0';

            size_t new_len = apdu->lc - old_len;
            memcpy(new_pin, apdu->data + old_len, new_len);
            new_pin[new_len] = '\0';

            if (pin_storage_openpgp_verify_pw1(old_pin)) {  // Level 3
                if (pin_storage_openpgp_change_pw1(new_pin)) {  // Level 3
                    ESP_LOGI(TAG, "PW1 changed successfully");
                    changed = true;
                    break;
                }
            }
        }

        // More nested code...
    } else if (pw_ref == 0x83) {  // Another Level 1 branch
        // Similar nested structure for PW3
    }
}
```

**Branching analysis:**
- 2 main branches (PW1 vs PW3)
- 2 nested length checks
- 1 for-loop per branch (variable iterations)
- 2 nested if-statements inside loop
- Multiple return statements

## Recommended Fix

1. **Extract PIN parsing logic** into a helper function:
```cpp
static bool try_change_pin(const uint8_t* data, size_t len,
                           size_t min_len,
                           bool (*verify_fn)(const char*),
                           bool (*change_fn)(const char*)) {
    for (size_t old_len = min_len; old_len <= len - min_len; old_len++) {
        char old_pin[OPENPGP_PIN_MAX_LEN + 1];
        char new_pin[OPENPGP_PIN_MAX_LEN + 1];

        memcpy(old_pin, data, old_len);
        old_pin[old_len] = '\0';

        size_t new_len = len - old_len;
        memcpy(new_pin, data + old_len, new_len);
        new_pin[new_len] = '\0';

        if (verify_fn(old_pin) && change_fn(new_pin)) {
            return true;
        }
    }
    return false;
}
```

2. **Simplify main function** using early returns:
```cpp
static int cmd_change_reference_data(const apdu_t *apdu, uint8_t *resp, size_t resp_max) {
    if (apdu->lc == 0) {
        return apdu_sw(resp, SW_WRONG_LENGTH);
    }

    bool (*verify_fn)(const char*);
    bool (*change_fn)(const char*);
    size_t min_len;

    if (apdu->p2 == 0x81) {  // PW1
        verify_fn = pin_storage_openpgp_verify_pw1;
        change_fn = pin_storage_openpgp_change_pw1;
        min_len = OPENPGP_PW1_MIN_LEN;
    } else if (appu->p2 == 0x83) {  // PW3
        verify_fn = pin_storage_openpgp_verify_pw3;
        change_fn = pin_storage_openpgp_change_pw3;
        min_len = OPENPGP_PW3_MIN_LEN;
    } else {
        return apdu_sw(resp, SW_INCORRECT_P1P2);
    }

    if (apdu->lc < min_len * 2) {
        return apdu_sw(resp, SW_WRONG_LENGTH);
    }

    if (try_change_pin(apdu->data, apdu->lc, min_len, verify_fn, change_fn)) {
        return apdu_sw(resp, SW_OK);
    }

    uint8_t retries = ...;  // Get retries based on pw_ref
    return apdu_sw(resp, retries == 0 ? SW_AUTH_METHOD_BLOCKED : 0x63C0 | retries);
}
```

**Estimated effort:** ~1 hour to refactor

## References

- Deep nesting anti-pattern: https://refactoring.com/catalog/extractMethod.html
- Guard clauses: https://refactoring.com/catalog/introduceExplainingVariable.html
- Strategy pattern for callback selection
