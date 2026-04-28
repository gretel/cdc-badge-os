---
title: "[LOW] Missing validation for empty string in typeString callback"
severity: LOW
domain: cdc_core
lens: type-safety
labels:
  - "audit:code-quality/type-safety"
---

## Summary
The `IKeyboardProvider::typeString` function accepts a `const char* text` parameter without explicit null-termination validation. While the function likely handles empty strings, there's no guarantee that callers pass valid, null-terminated strings.

## Evidence
In `components/cdc_core/include/cdc_core/IKeyboardProvider.h` line 39:
```cpp
virtual bool typeString(const char* text, uint16_t delayMs = 50) = 0;
```

The function signature allows:
1. `nullptr` to be passed
2. Non-null-terminated strings (if caller is careless)

## Impact
1. **Undefined behavior**: If `nullptr` is passed, `strlen` or string iteration may crash.
2. **Silent failures**: Empty strings may be handled inconsistently across implementations.

## Recommended Fix
Add explicit validation in the default implementation:

```cpp
bool KeyboardProvider::typeString(const char* text, uint16_t delayMs) {
    if (!text) {
        return false;  // Or log warning and return
    }
    
    // Optional: Check for empty string explicitly
    if (text[0] == '\0') {
        return true;  // Nothing to type
    }
    
    // ... rest of implementation
}
```

## References
- C++ Core Guidelines F.46: "Use 'const' and 'constexpr' to help avoid unintended conversions and reassignments"

</content>