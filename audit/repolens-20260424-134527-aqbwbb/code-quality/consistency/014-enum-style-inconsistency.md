---
title: "[LOW] Enum Declaration Style Inconsistency"
severity: LOW
domain: code-style
lens: code-consistency
labels:
  - "audit:code-quality/consistency"
---

## Summary
The codebase uses inconsistent enum declaration styles:

1. **cdc_core**: `enum class` (scoped enums with type)
2. **CalEPD**: `enum` (unscoped enums, used inline in function signatures)

### Evidence

**cdc_core (enum class - consistent):**
```cpp
// components/cdc_core/include/cdc_core/IService.h
enum class ServiceState : uint8_t {
    UNINITIALIZED,
    INITIALIZED,
    STARTED,
};

// components/cdc_core/include/cdc_core/IModule.h
enum class MenuLocation : uint8_t {
    MAIN_MENU,
    TOOLS_MENU,
    SETTINGS_MENU,
};

// components/cdc_core/include/cdc_core/EventBus.h
enum class EventType : uint8_t {
    KEY_PRESSED,
    KEY_RELEASED,
};

// components/cdc_core/include/cdc_core/UsbManager.h
enum class UsbHidInterface : uint8_t {
    Fido = 0,
    Keyboard = 1,
    Ccid = 2,
};
```

**CalEPD (enum - inline usage):**
```cpp
// components/CalEPD/include/parallel/ED060SC4.h
void update(enum EpdDrawMode mode = MODE_GC16);
void updateWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h, enum EpdDrawMode mode = MODE_EPDIY_BLACK_TO_GL16, bool using_rotation = true);

// components/CalEPD/include/parallel/ED047TC1touch.h
void update(enum EpdDrawMode mode = MODE_GC16);
void updateWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h, enum EpdDrawMode mode = MODE_GC16);

// components/CalEPD/include/parallel/ED047TC1.h
void update(enum EpdDrawMode mode = MODE_GC16);
void updateWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h, enum EpdDrawMode mode = MODE_GC16);

// components/CalEPD/models/parallel/ED060SC4.cpp
void Ed060SC4::update(enum EpdDrawMode mode)
void Ed060SC4::updateWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h, enum EpdDrawMode mode, bool using_rotation)
```

**Key differences:**
- `enum class` vs. `enum` (scoped vs. unscoped)
- Type specification: `: uint8_t` vs. implicit
- Inline enum declaration in function signatures vs. separate typedef
- `enum class` provides type safety, `enum` doesn't

## Impact
- **Type safety**: `enum class` provides better type checking
- **Namespace pollution**: `enum` values pollute enclosing scope
- **Modern C++**: `enum class` is preferred in C++11 and later
- **Readability**: `enum class` makes enum origin explicit (e.g., `ServiceState::STARTED`)

## Recommended Fix

**Establish and document a single convention:**

1. **Adopt `enum class`** (consistent with cdc_core and modern C++):
   - Better type safety
   - Scoped names (no pollution)
   - Explicit underlying type

2. **Files to fix in CalEPD (scope for ~1 hour fix):**
   - `components/CalEPD/include/parallel/ED060SC4.h`
   - `components/CalEPD/include/parallel/ED047TC1touch.h`
   - `components/CalEPD/include/parallel/ED047TC1.h`
   - Corresponding `.cpp` files

### Rename pattern:
```cpp
// BEFORE (unscoped enum)
void update(enum EpdDrawMode mode = MODE_GC16);
void updateWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h, enum EpdDrawMode mode = MODE_EPDIY_BLACK_TO_GL16, bool using_rotation = true);

// AFTER (scoped enum class)
void update(EpdDrawMode mode = EpdDrawMode::GC16);
void updateWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h, EpdDrawMode mode = EpdDrawMode::EPDIY_BLACK_TO_GL16, bool using_rotation = true);

// Enum definition:
enum class EpdDrawMode : uint8_t {
    GC16,
    EPDIY_BLACK_TO_GL16,
    // ...
};
```

## References
- C++ Core Guidelines: Use `enum class` instead of `enum`
- Modern C++: `enum class` is preferred since C++11
- cdc-badge-os project convention: `enum class` used in cdc_core, cdc_ui, cdc_hal

</content>