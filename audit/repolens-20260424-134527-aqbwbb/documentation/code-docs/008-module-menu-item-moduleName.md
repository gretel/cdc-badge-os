---
title: "[MEDIUM] ModuleMenuItem::moduleName field documentation missing"
severity: LOW
domain: documentation/code-docs
lens: code-docs
labels:
  - "audit:documentation/code-docs"
---

## Summary

In `IModule.h`, the `ModuleMenuItem` struct has a `moduleName` field documented as "Owner module name (set automatically)" but it's unclear WHAT sets it automatically and WHEN. This is important for module authors.

**File:** `components/cdc_core/include/cdc_core/IModule.h:32`

## Impact

- Module authors may manually set this field incorrectly
- Unclear if this is populated by the registry or by the module's `getMenuItems()` implementation

## Evidence

```cpp
// Line 32: Unclear documentation
struct ModuleMenuItem {
    // ...
    const char* moduleName;         ///< Owner module name (set automatically)
    // ...
};
```

Looking at the code flow:
1. Module::getMenuItems() returns array of ModuleMenuItem
2. ModuleRegistry::getMenuItems() collects these and sorts them
3. But who sets `moduleName`?

## Recommended Fix

Clarify the documentation to explain when/how this field is populated:

```cpp
/**
 * \brief Module menu item definition.
 *
 * Modules return arrays of these from getMenuItems().
 * Registry sorts by priority and places in appropriate menus.
 *
 * Note: moduleName field is typically set by the module's getMenuItems()
 * implementation (e.g., std::snprintf into a static buffer). The registry
 * does NOT populate this automatically.
 */
struct ModuleMenuItem {
    const char* label;              ///< Display label (use I18n for translation)
    uint8_t priority;               ///< Sort order (lower = higher in list, 0 = top)
    ui::IView* (*getView)();        ///< View factory (push on select)
    bool (*isVisible)();            ///< Visibility check (nullptr = always visible)
    const char* moduleName;         ///< Owner module name (module sets this in getMenuItems)
    MenuLocation location;          ///< Menu location (MAIN_MENU, TOOLS_MENU, etc.)
    void (*onSelect)();             ///< Action callback (used when getView is nullptr)
};
```

Alternatively, if the registry DOES populate this field, the code should be updated and documented:

```cpp
/**
 * \brief Module menu item definition.
 *
 * Modules return arrays of these from getMenuItems().
 * Registry sorts by priority, places in appropriate menus,
 * AND populates the moduleName field automatically.
 */
struct ModuleMenuItem {
    const char* label;              ///< Display label (use I18n for translation)
    uint8_t priority;               ///< Sort order (lower = higher in list, 0 = top)
    ui::IView* (*getView)();        ///< View factory (push on select)
    bool (*isVisible)();            ///< Visibility check (nullptr = always visible)
    const char* moduleName;         ///< Owner module name (set by registry after getMenuItems)
    MenuLocation location;          ///< Menu location (MAIN_MENU, TOOLS_MENU, etc.)
    void (*onSelect)();             ///< Action callback (used when getView is nullptr)
};
```

## References

- Related: `ModuleRegistry::getMenuItems()` implementation should clarify this
