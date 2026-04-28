---
title: "[LOW] PinManager PIN validation doesn't handle empty string edge case"
severity: LOW
domain: cdc_core/PinManager
lens: edge-case-testing
labels:
  - "audit:testing/edge-cases"
---

## Summary
In `PinManager::setBadgePin`, `setPW1`, and `setPW3` functions (file: `components/cdc_core/src/PinManager.cpp`), empty strings ("") pass the null check but fail silently or produce confusing behavior.

At line 347-355:
```cpp
bool PinManager::setBadgePin(const char* newPin) {
    if (!newPin) return false;  // Empty string "" passes this check
    size_t len = strlen(newPin);
    if (len < BADGE_PIN_MIN || len > BADGE_PIN_MAX) {
        LOG_E(TAG, "Badge PIN must be %d-%d digits", BADGE_PIN_MIN, BADGE_PIN_MAX);
        return false;
    }
    // ...
}
```

An empty string `""` has `len = 0`, which is less than `BADGE_PIN_MIN` (4), so it correctly rejects. However, the error message says "must be 4-8 digits" but doesn't explicitly handle the case where user might accidentally enter no PIN (e.g., from a UI that defaults to empty).

## Impact
- **UX clarity**: Empty string handling could be more explicit
- **Security**: Minor - empty PINs are rejected but the flow could be clearer

## Evidence
File: `components/cdc_core/src/PinManager.cpp`

Lines 347-362 (setBadgePin):
```cpp
bool PinManager::setBadgePin(const char* newPin) {
    if (!newPin) return false;
    size_t len = strlen(newPin);
    if (len < BADGE_PIN_MIN || len > BADGE_PIN_MAX) {
        LOG_E(TAG, "Badge PIN must be %d-%d digits", BADGE_PIN_MIN, BADGE_PIN_MAX);
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if (newPin[i] < '0' || newPin[i] > '9') {
            LOG_E(TAG, "PIN must contain only digits");
            return false;
        }
    }
    // ...
}
```

Lines 452-457 (setPW1):
```cpp
bool PinManager::setPW1(const char* newPin) {
    if (!newPin) return false;
    size_t len = strlen(newPin);
    if (len < PW1_MIN || len > PIN_MAX) {
        LOG_E(TAG, "PW1 must be %d-%d digits", PW1_MIN, PIN_MAX);
        return false;
    }
    // ...
}
```

Lines 550-556 (setPW3):
```cpp
bool PinManager::setPW3(const char* newPin) {
    if (!newPin) return false;
    size_t len = strlen(newPin);
    if (len < PW3_MIN || len > PIN_MAX) {
        LOG_E(TAG, "PW3 must be %d-%d digits", PW3_MIN, PIN_MAX);
        return false;
    }
    // ...
}
```

## Recommended Fix
Add explicit handling for empty strings to provide clearer error messages:

```cpp
bool PinManager::setBadgePin(const char* newPin) {
    if (!newPin) return false;
    size_t len = strlen(newPin);
    if (len == 0) {
        LOG_E(TAG, "PIN cannot be empty");
        return false;
    }
    if (len < BADGE_PIN_MIN || len > BADGE_PIN_MAX) {
        LOG_E(TAG, "Badge PIN must be %d-%d digits", BADGE_PIN_MIN, BADGE_PIN_MAX);
        return false;
    }
    // ...
}
```

Apply same pattern to `setPW1` and `setPW3`.

## References
- CWE-676: Use of Potentially Dangerous Function
- OWASP: Input Validation Cheat Sheet
