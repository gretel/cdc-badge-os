---
title: "[MEDIUM] Hardcoded PIN length and retry constants"
severity: MEDIUM
domain: code-quality
lens: magic-values
labels:
  - "audit:code-quality/magic-values"
---

## Summary
Multiple magic values related to PIN configuration are used in `components/cdc_views/src/PinEntryView.cpp` without centralized named constants. These include minimum PIN length (4), message display durations (1500ms, 3000ms), and lockout refresh interval (1000ms).

**Locations:**
- `components/cdc_views/src/PinEntryView.cpp:43` - `minLength_ = 4`
- `components/cdc_views/src/PinEntryView.cpp:111` - `if (nowMs - lastUpdate >= 1000)`
- `components/cdc_views/src/PinEntryView.cpp:133` - `showMessage(..., 1500)`
- `components/cdc_views/src/PinEntryView.cpp:161` - `showMessage(..., 3000)`

## Impact
- **Maintainability**: PIN constraints are defined in `PinManager.h` but the view uses hardcoded values that may drift from the intended configuration.
- **Consistency**: Message durations (1500ms, 3000ms) are used inline without explanation.
- **Configuration**: The minimum PIN length of 4 is set in the view instead of being derived from `PinManager::BADGE_PIN_MIN`.

## Evidence

**Line 43:**
```cpp
minLength_ = 4;  // Default minimum
```

**Line 111:**
```cpp
if (nowMs - lastUpdate >= 1000) {
    lastUpdate = nowMs;
    dirty_ = true;
}
```

**Lines 133, 161, 165:**
```cpp
showMessage(tr(StringId::PIN_TOO_SHORT), MessageIcon::WARNING, 1500);
showMessage(tr(StringId::LOCKED_OUT), MessageIcon::ERROR, 3000);
showMessage(tr(StringId::WRONG_PIN), MessageIcon::ERROR, 1500);
```

Note: While `PinManager.h` defines constraints like `BADGE_PIN_MIN = 4`, `PW1_MIN = 6`, `PW3_MIN = 8`, the `PinEntryView` uses a hardcoded `4` instead of referencing these constants.

## Recommended Fix

1. Define named constants for message durations near other layout constants:
```cpp
/** \brief Message display durations. */
static constexpr uint32_t MESSAGE_DURATION_SHORT = 1500;  // 1.5 seconds
static constexpr uint32_t MESSAGE_DURATION_LONG = 3000;   // 3 seconds
static constexpr uint32_t LOCKOUT_REFRESH_MS = 1000;      // 1 second
```

2. Update the view to use `PinManager` constants for PIN length:
```cpp
minLength_ = core::PinManager::BADGE_PIN_MIN;  // Or make it configurable
```

3. Replace magic numbers with constants:
```cpp
if (nowMs - lastUpdate >= LOCKOUT_REFRESH_MS) { ... }
showMessage(..., MESSAGE_DURATION_SHORT);
showMessage(..., MESSAGE_DURATION_LONG);
```

## References
- `components/cdc_core/include/cdc_core/PinManager.h` - Central PIN configuration
- [Magic Numbers (Code Quality Best Practices)](https://en.wikipedia.org/wiki/Magic_number_(programming))
