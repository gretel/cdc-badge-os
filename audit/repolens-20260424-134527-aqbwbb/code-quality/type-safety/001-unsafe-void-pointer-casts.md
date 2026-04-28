---
title: "[MEDIUM] Unsafe void* pointer casts for storing integer indices"
severity: MEDIUM
domain: type-safety
lens: c++-casting
labels:
  - "audit:code-quality/type-safety"
---

## Summary
Multiple locations in the codebase use `reinterpret_cast<void*>(static_cast<uintptr_t>(...))` to store integer indices in `void* userData` fields. This pattern, while commonly used for callback user data, bypasses type safety and can lead to:
- Loss of type information at callback sites
- Potential truncation on different architectures
- Confusion about whether the pointer is actually a pointer or an encoded integer

**Locations:**
- `components/mod_totp/src/TotpModule.cpp:711` - TOTP list item userData
- `components/mod_password/src/PasswordModule.cpp` - Password list item userData
- `components/mod_vcard/src/VcardModule.cpp` - Peer list item userData
- `components/mod_fido2/src/Fido2Ui.cpp` - FIDO2 list item userData
- `components/cdc_os_ui/src/ExpertMenuUi.cpp:43, 85` - Module retry index encoding

## Impact
**Maintenance burden**: Developers reading the code must understand the implicit encoding scheme to correctly interpret `userData` values.

**Subtle bugs**: If a developer mistakenly treats the encoded integer as a real pointer (e.g., dereferencing it), undefined behavior occurs.

**Portability**: While `uintptr_t` ensures sufficient width, the pattern is architecture-dependent and may confuse static analysis tools.

## Evidence
```cpp
// components/mod_totp/src/TotpModule.cpp:711
s_listItems[idx].userData = reinterpret_cast<void*>(static_cast<uintptr_t>(logical));

// components/cdc_os_ui/src/ExpertMenuUi.cpp:43
static void onModuleRetryConfirm(void* userData) {
    uint8_t index = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(userData));
    // ...
}
```

## Recommended Fix
Consider using a typed struct for list items instead of generic `void* userData`:

```cpp
struct TypedListItem {
    const char* label;
    uint8_t icon;
    bool iconDisabled;
    uint16_t indexData;  // For integer indices
    const void* ptrData; // For actual pointers
    bool isIndex;        // Disambiguate
};
```

Alternatively, use `std::variant<uint16_t, const void*>` (C++17) for type-safe user data:

```cpp
struct ListItem {
    const char* label;
    uint8_t icon;
    bool iconDisabled;
    std::variant<uint16_t, const void*> userData;
};
```

For simple cases, document the convention clearly:

```cpp
struct ListItem {
    const char* label;
    uint8_t icon;
    bool iconDisabled;
    void* userData;  // NOTE: For small integers, use static_cast<void*>(uintptr_t(val))
};
```

## References
- C++ Core Guidelines, C.19: "If a function can take an argument of several different types, use a variant"
- C++ Core Guidelines, Expr.13: "Use static_cast for numeric conversions, reinterpret_cast for bit-level reinterpreting"
