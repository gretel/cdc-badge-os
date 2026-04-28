---
title: "[LOW] LockScreenContextItem struct missing documentation"
severity: LOW
domain: documentation/code-docs
lens: code-docs
labels:
  - "audit:documentation/code-docs"
---

## Summary

The `LockScreenContextItem` struct in `components/cdc_core/include/cdc_core/IModule.h:40-47` lacks documentation.

**File:** `components/cdc_core/include/cdc_core/IModule.h:40-47`

## Impact

- Unclear when context menu items appear
- No guidance on how labels are displayed

## Evidence

```cpp
// Line 40-47: Undocumented struct
struct LockScreenContextItem {
    const char* (*getLabel)();      ///< No docs
    void (*callback)();             ///< No docs
    uint8_t priority;               ///< No docs
    const char* moduleName;         ///< No docs
};
```

## Recommended Fix

Add documentation:

```cpp
/**
 * \brief Lock screen context menu item.
 *
 * Shown in context menu when lock screen is active.
 * Labels can be dynamic (e.g., show current TOTP code).
 */
struct LockScreenContextItem {
    const char* (*getLabel)();      ///< Dynamic label getter
    void (*callback)();             ///< Action when selected
    uint8_t priority;               ///< Sort order (lower = higher)
    const char* moduleName;         ///< Owner module name (set automatically)
};
```

## References

- Related: `IModule::getLockScreenContextItems()` uses this struct
