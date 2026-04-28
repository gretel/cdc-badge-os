---
title: "[LOW] Boolean method naming: inconsistent use of is/has/can prefixes"
severity: LOW
domain: cdc_hal
lens: naming-conventions
labels:
  - "audit:code-quality/naming"
---

## Summary
Boolean methods in the codebase use inconsistent naming prefixes. Some use `is` (`isKeyPressed`, `isBusy`), others use `has` (`hasKey`), and some use no prefix (`anyKeyDown`, `needsRender`).

**Evidence**:
```cpp
// IKeypad.h - mixed boolean prefixes
virtual bool isKeyPressed(Key key) const = 0;  // is prefix
virtual bool hasKey() const = 0;               // has prefix
virtual bool anyKeyDown() const = 0;           // no prefix

// IDisplay.h - mixed boolean prefixes
virtual bool isBusy() const = 0;               // is prefix
virtual bool isBacklightOn() const = 0;        // is prefix

// IView.h - inconsistent boolean prefixes
virtual bool needsRender() const = 0;          // needs prefix
virtual bool needsRender() const = 0;          // needs prefix

// PinManager.h - mixed boolean prefixes
bool isBadgeBlocked() const;                   // is prefix
bool isPW1Blocked() const;                     // is prefix
bool isStorageAvailable() const;               // is prefix
```

## Impact
- **Readability**: Inconsistent prefixes make it unclear what the method returns
- **Discoverability**: Developers may search for `hasKey()` instead of `anyKeyDown()`
- **Maintainability**: Unclear which pattern to follow for new boolean methods

## Evidence
- File: `components/cdc_hal/include/cdc_hal/IKeypad.h`
- File: `components/cdc_hal/include/cdc_hal/IDisplay.h`
- File: `components/cdc_ui/include/cdc_ui/IView.h`
- File: `components/cdc_core/include/cdc_core/PinManager.h`
- Inconsistent patterns: `isKeyPressed`, `hasKey`, `anyKeyDown`, `needsRender`

## Recommended Fix
Standardize boolean method naming using clear prefixes:

```cpp
// Use is/are for state checks
virtual bool isKeyPressed(Key key) const = 0;
virtual bool isBusy() const = 0;
virtual bool isBacklightOn() const = 0;
virtual bool isBadgeBlocked() const;
virtual bool isStorageAvailable() const;

// Use has for ownership/existence checks
virtual bool hasKey() const = 0;
virtual bool hasSlotRange() const;

// Use can/could for capability checks
virtual bool canRender() const;

// Use specific action-based names for queries
virtual bool anyKeyDown() const = 0;  // Keep as is - it's clear
virtual bool needsRender() const = 0; // Keep as is - it's clear
```

## References
- C++ Core Guidelines: [C.35: Use a consistent naming style](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#C35)
- Google C++ Style Guide: [Boolean names](https://google.github.io/styleguide/cppguide.html#Boolean_Names) - use "is", "has", "are" prefixes
