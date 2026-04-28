---
title: "[LOW] Over-documentation with trivial getter comments in PinManager"
severity: LOW
domain: code-quality/comments
lens: comments
labels:
  - "audit:code-quality/comments"
---

## Summary
The `PinManager` class in `components/cdc_core/include/cdc_core/PinManager.h` has getter methods with trivial inline comments that add no value:

```cpp
uint8_t getBadgeRetries() const { return badgeRetries_; }
bool isBadgeBlocked() const;  // Checks retries=0 OR time lockout active
void resetBadgeRetries();

// === Lockout Timer (RAM only, not persistent) ===
static constexpr uint32_t LOCKOUT_DURATION_MS = 60000;  // 60 seconds
void startLockout();
uint32_t getLockoutRemainingMs() const;
bool isLockoutActive() const;
```

The comments either:
- Restate what the method name already clearly indicates
- Only apply to some methods in a group, creating inconsistency

## Impact
- **Visual noise**: Trivial comments make the API harder to scan
- **Inconsistency**: Some getters have comments, others don't (e.g., `getBadgeRetries()` has none, `isBadgeBlocked()` has one)
- **Maintenance**: Comments like "60 seconds" duplicate the value `60000` (easy to get out of sync)

## Evidence
**File**: `components/cdc_core/include/cdc_core/PinManager.h`  
**Lines 68-76**:
```cpp
uint8_t getBadgeRetries() const { return badgeRetries_; }
bool isBadgeBlocked() const;  // Checks retries=0 OR time lockout active
void resetBadgeRetries();

// === Lockout Timer (RAM only, not persistent) ===
static constexpr uint32_t LOCKOUT_DURATION_MS = 60000;  // 60 seconds
void startLockout();
uint32_t getLockoutRemainingMs() const;
bool isLockoutActive() const;
```

## Recommended Fix
Remove trivial comments from obvious getters. Keep comments only for non-obvious behavior:

```cpp
uint8_t getBadgeRetries() const { return badgeRetries_; }
bool isBadgeBlocked() const;  // Returns true if retries exhausted OR lockout timer active
void resetBadgeRetries();

static constexpr uint32_t LOCKOUT_DURATION_MS = 60000;  // milliseconds
void startLockout();
uint32_t getLockoutRemainingMs() const;
bool isLockoutActive() const;
```

Or better, move the explanatory comment to the method declaration itself:
```cpp
/**
 * Check if badge is blocked (retries exhausted or lockout timer active)
 */
bool isBadgeBlocked() const;
```

## References
- Clean Code (Robert Martin): "Comments should explain 'why', not 'what'"
- C++ Core Guidelines: "Self-documenting code is better than obvious comments"
