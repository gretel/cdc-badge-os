---
title: "[014] [MEDIUM] Unnecessary const_cast in PinManager::isLockoutActive"
severity: MEDIUM
domain: code-readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
In `components/cdc_core/src/PinManager.cpp:646-663`, the `isLockoutActive()` method is declared `const` but uses `const_cast` to modify member variables for lazy state update:

```cpp
bool PinManager::isLockoutActive() const {
    if (!lockoutActive_) {
        return false;
    }

    uint32_t remaining = getLockoutRemainingMs();
    if (remaining == 0) {
        // Lockout expired - reset retries (const_cast needed for lazy update)
        const_cast<PinManager*>(this)->lockoutActive_ = false;
        const_cast<PinManager*>(this)->badgeRetries_ = MAX_RETRIES;
        const_cast<PinManager*>(this)->saveToStorage();
        LOG_I(TAG, "Lockout expired, retries reset");
        return false;
    }
    return true;
}
```

The comment acknowledges this is a "lazy update" pattern, but the const_cast makes it look like the method is doing something unusual.

## Impact
- **Visual alarm**: `const_cast` typically signals something unusual or potentially unsafe
- **Cognitive load**: Reader must understand why a const method needs to mutate state
- **Hidden side effects**: A `const` method calling `saveToStorage()` is not obvious from the signature

## Evidence
**File**: `components/cdc_core/src/PinManager.cpp:646-663`

The method is a "getter" that has a side effect of persisting state. The lazy update pattern is valid but poorly communicated.

## Recommended Fix
Either:
1. **Make the method non-const** (most explicit):
```cpp
bool PinManager::isLockoutActive() {  // Remove const
    if (!lockoutActive_) {
        return false;
    }

    uint32_t remaining = getLockoutRemainingMs();
    if (remaining == 0) {
        lockoutActive_ = false;
        badgeRetries_ = MAX_RETRIES;
        saveToStorage();
        LOG_I(TAG, "Lockout expired, retries reset");
        return false;
    }
    return true;
}
```

2. **Use mutable for the cache fields** (if they're truly cache-only):
```cpp
mutable bool lockoutActive_ = false;
mutable uint32_t lockoutStartMs_ = 0;

bool PinManager::isLockoutActive() const {
    // Now const_cast is not needed
    if (!lockoutActive_) {
        return false;
    }

    uint32_t remaining = getLockoutRemainingMs();
    if (remaining == 0) {
        lockoutActive_ = false;
        badgeRetries_ = MAX_RETRIES;
        // saveToStorage() still needs const_cast though...
    }
}
```

3. **Extract the lazy update to a separate non-const method**:
```cpp
bool PinManager::isLockoutActive() const {
    if (!lockoutActive_) {
        return false;
    }
    return getLockoutRemainingMs() > 0;
}

void PinManager::checkAndResetExpiredLockout() {  // Non-const, explicit side effect
    if (isLockoutActive() && getLockoutRemainingMs() == 0) {
        lockoutActive_ = false;
        badgeRetries_ = MAX_RETRIES;
        saveToStorage();
        LOG_I(TAG, "Lockout expired, retries reset");
    }
}
```

Option 3 is the cleanest - it separates the query from the mutation.

## References
- [C++ Core Guidelines - C.59: Use mutable only when it makes sense for the logical state of an object](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#C59-use-mutable-only-when-it-makes-sense-for-the-logical-state-of-an-object)
- [Effective C++ - Item 3: Make const methods truly const](https://www.oreilly.com/library/view/effective-c-2nd/0201383578/ch03.html)
