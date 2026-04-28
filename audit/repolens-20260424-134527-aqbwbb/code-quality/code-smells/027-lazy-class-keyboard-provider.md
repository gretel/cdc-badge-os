---
title: "[LOW] Lazy Class: IKeyboardProvider interface has single implementation"
severity: LOW
domain: cdc_core
lens: code-smells
labels:
  - "refactor:remove-indirection"
  - "simplicity"
---

## Summary
The `IKeyboardProvider` interface exists but appears to have only one implementation, making it an unnecessary layer of indirection.

**Location:** `components/cdc_core/include/cdc_core/IKeyboardProvider.h` and `components/cdc_core/src/IKeyboardProvider.cpp`

## Evidence
```cpp
// IKeyboardProvider.h - Simple interface
#pragma once
#include <cstdint>

namespace cdc::core {

class IKeyboardProvider {
public:
    virtual ~IKeyboardProvider() = default;
    virtual const char* getKeyboardLayout() const = 0;
    virtual char getCharAt(uint8_t row, uint8_t col) const = 0;
    virtual uint8_t getRowCount() const = 0;
    virtual uint8_t getColCount() const = 0;
};

} // namespace cdc::core
```

```cpp
// IKeyboardProvider.cpp - Minimal implementation
#include "cdc_core/IKeyboardProvider.h"

namespace cdc::core {

IKeyboardProvider::~IKeyboardProvider() = default;

} // namespace cdc::core
```

The interface has:
- No virtual destructor implementation (just `= default`)
- No non-virtual methods with shared behavior
- Only pure virtual methods (interface-only)

If there's only one implementation, this is a "lazy class" - a class that does too little to justify its existence.

## Impact
- **Unnecessary indirection**: Adds complexity without benefit
- **Boilerplate**: Requires implementing 4 methods even for simple cases
- **Indirect usage**: Callers must go through interface pointer

## Recommended Fix
If only one implementation exists, consider inlining the interface:

```cpp
// Option 1: Inline directly into a concrete class
class T9Keyboard {
public:
    const char* getKeyboardLayout() const { return "T9"; }
    char getCharAt(uint8_t row, uint8_t col) const;
    uint8_t getRowCount() const { return 4; }
    uint8_t getColCount() const { return 3; }
};

// Option 2: Keep interface but document future extensibility
/**
 * IKeyboardProvider - Interface for multi-layout keyboard support
 * Current: Single T9 implementation
 * Future: May add QWERTY, AZERTY, etc.
 */
```

If multiple implementations are planned:
```cpp
// Keep interface but add factory
class IKeyboardProvider {
public:
    virtual ~IKeyboardProvider() = default;
    virtual const char* getKeyboardLayout() const = 0;
    virtual char getCharAt(uint8_t row, uint8_t col) const = 0;
    virtual uint8_t getRowCount() const = 0;
    virtual uint8_t getColCount() const = 0;

    // Factory for future extensibility
    static std::unique_ptr<IKeyboardProvider> create(LayoutType type);
};
```

## References
- Martin Fowler, "Refactoring: Improving the Design of Existing Code" - Lazy Class smell
- YAGNI (You Ain't Gonna Need It) - Add functionality only when needed
