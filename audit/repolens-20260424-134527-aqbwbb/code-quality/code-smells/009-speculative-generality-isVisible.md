---
title: "[MEDIUM] Speculative Generality: `isVisible` callback in ModuleMenuItem may be over-engineered"
severity: MEDIUM
domain: core
lens: code-smells
labels:
  - "speculative-generality"
  - "cdc_core"
---

## Summary
In `components/cdc_core/include/cdc_core/IModule.h:28-36`, the `ModuleMenuItem` struct has an `isVisible` callback pointer that allows dynamic visibility checking. While useful, this pattern may be speculative generality if visibility is rarely dynamic.

## Impact
**Complexity**: Every menu item can have a callback function pointer.

**Memory overhead**: Function pointers take space even when not used.

**Indirection**: Menu rendering must check and call the function.

**Unclear usage**: Hard to tell at a glance which items have dynamic visibility.

## Evidence
`components/cdc_core/include/cdc_core/IModule.h:28-36`:
```cpp
struct ModuleMenuItem {
    const char* label;              // Display label (use I18n for static)
    uint8_t priority;               // Sort order (lower = higher in list)
    ui::IView* (*getView)();        // Factory function to get the view (push view on select)
    bool (*isVisible)();            // Optional visibility check (nullptr = always visible)
    const char* moduleName;         // Owner module name (set automatically)
    MenuLocation location;          // Where to show this item
    void (*onSelect)();             // Toggle/action callback (used when getView is nullptr)
};
```

The comment says "Optional visibility check (nullptr = always visible)" but it's unclear how often this is actually used dynamically.

Looking at `Fido2Module.cpp:226-232`:
```cpp
items[0] = {fido2_ui_get_label(), 50, []() -> ui::IView* {
    return fido2_ui_get_list_view();
}, nullptr, getName(), core::MenuLocation::MAIN_MENU, nullptr};
```

Most items use `nullptr` for `isVisible`.

## Recommended Fix
1. **Add a flag instead of callback** if visibility is mostly static:
```cpp
struct ModuleMenuItem {
    const char* label;
    uint8_t priority;
    ui::IView* (*getView)();
    bool visible;  // Simple flag for common case
    const char* moduleName;
    MenuLocation location;
    void (*onSelect)();
};
```

2. **Keep callback** but only for modules that truly need dynamic visibility:
```cpp
struct ModuleMenuItem {
    const char* label;
    uint8_t priority;
    ui::IView* (*getView)();
    bool (*getVisibility)();  // Renamed for clarity
    const char* moduleName;
    MenuLocation location;
    void (*onSelect)();
    bool hasDynamicVisibility;  // Flag to indicate callback is valid
};
```

**Estimated effort**: ~1 hour to refactor the struct and update all usages.

## References
- Refactoring.com: "Speculative Generality" - https://refactoring.com/catalog/removeParameter
- Martin Fowler, "Refactoring: Improving the Design of Existing Code", Chapter 7
