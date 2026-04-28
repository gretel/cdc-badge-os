---
title: "[LOW] ModuleMenuItem struct missing documentation"
severity: LOW
domain: documentation/code-docs
lens: code-docs
labels:
  - "audit:documentation/code-docs"
---

## Summary

The `ModuleMenuItem` struct in `components/cdc_core/include/cdc_core/IModule.h:27-37` lacks documentation describing each field's purpose and constraints.

**File:** `components/cdc_core/include/cdc_core/IModule.h:27-37`

## Impact

- Developers may misunderstand field purposes (e.g., priority sorting)
- Unclear what happens if `getView` and `onSelect` are both set

## Evidence

```cpp
// Line 27-37: Undocumented struct
struct ModuleMenuItem {
    const char* label;              ///< No docs
    uint8_t priority;               ///< No docs - what range?
    ui::IView* (*getView)();        ///< No docs
    bool (*isVisible)();            ///< No docs
    const char* moduleName;         ///< No docs
    MenuLocation location;          ///< No docs
    void (*onSelect)();             ///< No docs - when is this used instead of getView?
};
```

## Recommended Fix

Add documentation:

```cpp
/**
 * \brief Module menu item definition.
 *
 * Modules return arrays of these from getMenuItems().
 * Registry sorts by priority and places in appropriate menus.
 */
struct ModuleMenuItem {
    const char* label;              ///< Display label (use I18n for translation)
    uint8_t priority;               ///< Sort order (lower = higher in list, 0 = top)
    ui::IView* (*getView)();        ///< View factory (push on select)
    bool (*isVisible)();            ///< Visibility check (nullptr = always visible)
    const char* moduleName;         ///< Owner module name (set automatically)
    MenuLocation location;          ///< Menu location (MAIN_MENU, TOOLS_MENU, etc.)
    void (*onSelect)();             ///< Action callback (used when getView is nullptr)
};
```

## References

- Related: `MenuLocation` enum just above this struct
- Related: `LockScreenContextItem` struct at line 40
