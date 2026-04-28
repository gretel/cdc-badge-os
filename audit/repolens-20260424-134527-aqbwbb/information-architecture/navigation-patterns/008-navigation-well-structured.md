---
title: "[INFO] Navigation structure is well-organized with consistent patterns"
severity: INFO
domain: information-architecture/navigation-patterns
lens: navigation-patterns
labels:
  - "audit:information-architecture/navigation-patterns"
---

## Summary
The navigation architecture follows a clean, consistent hierarchical pattern with a well-defined view stack. The module-based menu system allows for extensibility and the separation of concerns is well-implemented.

**Files reviewed:**
- `components/cdc_ui/include/cdc_ui/ViewStack.h` - Navigation stack
- `components/cdc_os_ui/src/AppUi.cpp` - Main menu flow
- `components/cdc_core/include/cdc_core/IModule.h` - Module interface
- All module implementations (TOTP, FIDO2, Password, etc.)

## Impact
**Positive aspects:**
1. **Consistent navigation pattern** - All menus use ListView with same key bindings (2/8 scroll, Y select, N back)
2. **Modular architecture** - Modules register menu items dynamically without tight coupling
3. **Clear separation** - ViewStack manages navigation, modules provide content
4. **Reusable components** - ListView, ContextMenu, Toast, Confirm views used consistently
5. **Well-documented** - Code has good comments and Doxygen-style documentation

## Evidence
Consistent menu structure across all modules:

```cpp
// All modules follow same pattern:
// components/mod_totp/src/TotpModule.cpp (lines 986-1000)
items[0] = {mstr(STR_TOTP), 50, []() -> ui::IView* {
    rebuildList();
    return &s_listView;
}, nullptr, getName(), core::MenuLocation::MAIN_MENU, nullptr};
```

ViewStack provides clean navigation API:

```cpp
// components/cdc_ui/include/cdc_ui/ViewStack.h
void push(IView* view, void* context = nullptr);
void pop();
void replace(IView* view, void* context = nullptr);
void popToRoot();
void showModal(IView* modal);
void hideModal();
```

Module registration is clean and isolated:

```cpp
// components/cdc_core/include/cdc_core/IModule.h (lines 74-90)
virtual uint8_t getMenuItems(ModuleMenuItem* items, uint8_t maxItems);
virtual ui::IView* getEntryView();
virtual uint8_t getLockScreenContextItems(LockScreenContextItem* items, uint8_t maxItems);
```

## Recommendations
This is a **positive finding** - the navigation architecture is well-designed. Consider:
1. Documenting the navigation patterns for future developers
2. Creating a navigation diagram for the README
3. Adding the improvements suggested in other findings to further enhance UX

No immediate fix required - this is informational.

## References
- ViewStack: `components/cdc_ui/include/cdc_ui/ViewStack.h`
- Module interface: `components/cdc_core/include/cdc_core/IModule.h`
- Main menu: `components/cdc_os_ui/src/AppUi.cpp`
