---
title: "[MEDIUM] Boolean method naming: inconsistent use of is/has/can prefixes"
severity: MEDIUM
domain: naming-conventions
lens: code-quality
labels:
  - "audit:code-quality/naming"
---

## Summary
Boolean methods use inconsistent prefixes. Some methods use `isXxx()`, others use `hasXxx()`, `areXxx()`, or no prefix at all.

**Evidence:**

1. **components/cdc_core/include/cdc_core/PinManager.h** (lines 66-99):
   ```cpp
   bool isBadgeBlocked() const;
   bool isLockoutActive() const;
   bool isPinSet() const { return badgePinIsSet_; }
   bool isStorageAvailable() const;
   bool isPW1Blocked() const { return pw1Retries_ == 0; }
   bool isPW3Blocked() const { return pw3Retries_ == 0; }
   ```
   Uses `is` prefix consistently for boolean methods.

2. **components/cdc_hal/include/cdc_hal/IKeypad.h** (lines 40-55):
   ```cpp
   virtual bool isKeyPressed(Key key) const = 0;
   virtual Key getNextKey() = 0;
   virtual bool hasKey() const = 0;
   virtual bool anyKeyDown() const = 0;
   ```
   Mixed: `isKeyPressed()`, `hasKey()`, `anyKeyDown()` (no prefix)

3. **components/cdc_core/include/cdc_core/EventBus.h** (line 87):
   ```cpp
   bool init(size_t queueSize = DEFAULT_QUEUE_SIZE);
   ```
   No prefix for initialization method (correct).

4. **components/cdc_views/include/cdc_views/PinEntryView.h** (lines 106-112):
   ```cpp
   bool isLockedOut() const { return lockedOut_; }
   void clear();
   bool isPinSet() const;  // (example - check actual implementation)
   ```
   Uses `is` prefix.

5. **components/mod_password/include/mod_password/PasswordStore.h** (lines 58-62):
   ```cpp
   bool hasSlotRange() const { return hasSlotRange_; }
   ```
   Uses `has` prefix.

## Impact
- **Readability**: `isXxx()` clearly indicates boolean return type
- **Consistency**: Different prefixes for similar semantic meaning
- **Discoverability**: Developers may guess wrong method name

## Evidence
Comparison of similar semantic methods:
- `isBadgeBlocked()` vs `anyKeyDown()` - both check state, different naming
- `hasKey()` vs `isKeyPressed()` - both check key state, different prefixes
- `hasSlotRange()` - uses "has" for existence check (correct usage)

## Recommended Fix
Adopt consistent boolean method naming convention:

**Rules:**
1. **State checks**: Use `isXxx()` (e.g., `isLockedOut()`, `isBadgeBlocked()`)
2. **Existence checks**: Use `hasXxx()` (e.g., `hasKey()`, `hasSlotRange()`)
3. **Capability checks**: Use `canXxx()` (e.g., `canAuthenticate()`)
4. **Action methods**: No prefix (e.g., `clear()`, `init()`)
5. **Exception**: `anyKeyDown()` is OK - it's a common pattern

**Update IKeypad.h:**
```cpp
virtual bool isKeyPressed(Key key) const = 0;
virtual bool hasKey() const = 0;
virtual bool hasAnyKey() const = 0;  // or keep as anyKeyDown() - it's common
```

**Steps:**
1. Audit all boolean methods in the codebase
2. Apply consistent naming: `isXxx()` for state, `hasXxx()` for existence
3. Update method names and all call sites
4. Document the convention

## References
- [Google C++ Style Guide - Function Names](https://google.github.io/styleguide/cppguide.html#Function_Names)
- [C++ Core Guidelines - Naming](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-naming)
