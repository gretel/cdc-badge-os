---
title: "[LOW] Tropic01Element uses global singleton instead of class method"
severity: LOW
domain: modularity
lens: modularity
labels:
  - "audit:maintainability/modularity"
---

## Summary
The `Tropic01Element` class in `cdc_hal` uses a global static instance `g_secureElement` instead of providing a proper singleton method like the other modules. This makes the state accessible from anywhere in the compilation unit and prevents clean instantiation if needed.

**Locations:**
- `components/cdc_hal/src/Tropic01Element.cpp:880`

## Impact
**Testability:**
- Cannot create isolated test instances with different state
- Harder to mock for unit testing

**Memory efficiency:**
- State is always allocated even if TROPIC01 is disabled

**Consistency:**
- Inconsistent with other modules that use `::instance()` pattern

## Evidence
**Global singleton declaration (Tropic01Element.cpp, line ~880):**
```cpp
/** \brief Global singleton instance of TROPIC secure-element implementation. */
static Tropic01Element g_secureElement;

/**
 * \brief Returns the singleton secure element service instance.
 * \return Pointer to the global `ISecureElement` implementation.
 */
ISecureElement* getSecureElement() {
    return &g_secureElement;
}
```

**Comparison with other modules:**
```cpp
// FIDO2 module (consistent pattern)
class Fido2Module {
public:
    static Fido2Module& instance();
    // ...
};

// TROPIC01 (inconsistent pattern)
static Tropic01Element g_secureElement;
ISecureElement* getSecureElement() { return &g_secureElement; }
```

## Recommended Fix
Add a proper `instance()` method to `Tropic01Element` class:

**Step 1: Add instance() method to header (5 minutes)**
```cpp
// components/cdc_hal/include/cdc_hal/Tropic01Element.h
class Tropic01Element : public ISecureElement {
public:
    static Tropic01Element& instance();  // Add this
    // ... rest of class
};
```

**Step 2: Implement instance() method (5 minutes)**
```cpp
// components/cdc_hal/src/Tropic01Element.cpp
Tropic01Element& Tropic01Element::instance() {
    static Tropic01Element s_instance;
    return s_instance;
}
```

**Step 3: Update getSecureElement() to use instance() (5 minutes)**
```cpp
ISecureElement* getSecureElement() {
    return &Tropic01Element::instance();
}
```

**Step 4: Remove global variable (5 minutes)**
Remove `static Tropic01Element g_secureElement;` from the file.

**Step 5: Update callers (15 minutes)**
Update any code that uses `getSecureElement()` to use `Tropic01Element::instance()` directly if needed.

**Total estimated time: ~35 minutes**

## References
- Singleton pattern: https://en.wikipedia.org/wiki/Singleton_pattern
- C++ Core Guidelines: [F.23](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Fa-init) - Use classes for state management
- Existing project pattern: See `Fido2Module::instance()`, `GpgModule::instance()`
