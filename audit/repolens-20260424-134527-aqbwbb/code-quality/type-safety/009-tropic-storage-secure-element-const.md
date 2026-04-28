---
title: "[LOW] Missing const qualifier on ISecureElement pointer member"
severity: LOW
domain: cdc_core
lens: type-safety
labels:
  - "audit:code-quality/type-safety"
---

## Summary
The `TropicStorage` class stores a pointer to `ISecureElement` without a `const` qualifier, allowing accidental reassignment of the secure element instance.

## Evidence
In `components/cdc_core/include/cdc_core/TropicStorage.h` line 32:
```cpp
void setSecureElement(cdc::hal::ISecureElement* se) { secureElement_ = se; }
```

In `components/cdc_core/include/cdc_core/TropicStorage.h` line 73:
```cpp
cdc::hal::ISecureElement* secureElement_ = nullptr;
```

The member can be reassigned via `setSecureElement()`, but there's no guarantee it won't be changed unexpectedly.

## Impact
1. **Accidental reassignment**: The secure element pointer could be changed during operation, potentially causing data corruption.
2. **Type clarity**: It's not clear from the signature whether the pointer should be constant.

## Recommended Fix
If the secure element should not change after initialization:
1. Make the member `const`:
   ```cpp
   const cdc::hal::ISecureElement* secureElement_ = nullptr;
   ```

2. Or add a `setSecureElementOnce()` method that only works before initialization:
   ```cpp
   bool setSecureElementOnce(cdc::hal::ISecureElement* se);
   ```

If reassignment is intentional, add logging to track when it happens:
```cpp
void setSecureElement(cdc::hal::ISecureElement* se) {
    LOG_D(TAG, "Setting secure element from %p to %p", secureElement_, se);
    secureElement_ = se;
}
```

## References
- C++ Core Guidelines C.43: "Declare a pointer parameter that is not reassigned as 'const'"

</content>