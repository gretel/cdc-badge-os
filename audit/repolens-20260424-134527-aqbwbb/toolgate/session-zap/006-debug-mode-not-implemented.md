---
title: "[LOW] DEBUG_MODE flag documented but not implemented for lockouts"
severity: LOW
domain: security
lens: toolgate/session-zap
labels:
  - "audit:toolgate/session-zap"
---

## Summary

The `DEBUG_MODE` feature flag is documented as disabling PIN lockouts, but the actual implementation in `PinManager.cpp` does not check this flag before starting lockouts.

**Location:** `components/cdc_core/src/PinManager.cpp:325-326`

## Impact

**Documentation vs. implementation mismatch:**

**README.md (line 169):**
```
| `DEBUG_MODE` | 1 | Disables PIN lockouts and increases log verbosity. **Set to 0 for production!** |
```

**feature_flags.h (line 29-32):**
```cpp
// Debug Mode (disables lockouts, useful for development)
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif
```

**Actual implementation (PinManager.cpp:324-327):**
```cpp
// Start lockout timer when retries exhausted
if (badgeRetries_ == 0) {
    startLockout();  // <-- No DEBUG_MODE check!
}
```

**Effects:**
1. Lockouts work the same in DEBUG_MODE and production
2. Developers expecting `DEBUG_MODE=1` to disable lockouts may be confused
3. The flag affects logging verbosity but not lockout behavior
4. Inconsistent with documented behavior

## Evidence

**File: `components/cdc_core/src/PinManager.cpp`**

Line 320-327:
```cpp
badgeRetries_--;
saveToStorage();  // Persist retry count
LOG_W(TAG, "Wrong badge PIN, %d retries left", badgeRetries_);

// Start lockout timer when retries exhausted
if (badgeRetries_ == 0) {
    startLockout();  // Always called, regardless of DEBUG_MODE
}
```

**File: `README.md`**

Line 169:
```
`DEBUG_MODE` | 1 | Disables PIN lockouts and increases log verbosity.
```

**File: `feature_flags.h`**

Line 29-32:
```cpp
// Debug Mode (disables lockouts, useful for development)
#ifndef DEBUG_MODE
#define DEBUG_MODE 1
#endif
```

## Recommended Fix

Two options:

**Option 1: Implement DEBUG_MODE check in lockout logic**
```cpp
// Start lockout timer when retries exhausted
if (badgeRetries_ == 0) {
#if DEBUG_MODE
    LOG_I(TAG, "Lockout started (DEBUG_MODE=1, timer only)");
#else
    startLockout();
#endif
}
```

**Option 2: Update documentation to match actual behavior**
```markdown
| `DEBUG_MODE` | 1 | Increases log verbosity. Set to 0 for production! |
```

**Option 3: Add explicit DEBUG_MODE behavior**
```cpp
#if DEBUG_MODE
// In debug mode, reduce lockout duration for testing
static constexpr uint32_t LOCKOUT_DURATION_MS = 5000;  // 5 seconds
#else
static constexpr uint32_t LOCKOUT_DURATION_MS = 60000;  // 60 seconds
#endif
```

## References

- CWE-489: Active Debug Code
- [OWASP: Debugging Code](https://cheatsheetseries.owasp.org/cheatsheets/Debugging_Cheat_Sheet.html)
