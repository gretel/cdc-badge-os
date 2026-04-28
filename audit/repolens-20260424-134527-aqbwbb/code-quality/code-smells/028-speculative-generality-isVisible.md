---
title: "[LOW] Speculative Generality: isVisible() method rarely used"
severity: LOW
domain: cdc_core
lens: code-smells
labels:
  - "refactor:remove-unnecessary-abstraction"
  - "simplicity"
---

## Summary
The `ModuleMenuItem` struct includes an `isVisible()` function pointer that adds complexity but is rarely used in practice. This is speculative generality - adding flexibility "just in case" it's needed.

**Location:** `components/cdc_core/include/cdc_core/IModule.h:26-34`

## Evidence
```cpp
// IModule.h:26-34 - isVisible() function pointer
struct ModuleMenuItem {
    const char* label;              // Display label (use I18n for translation)
    uint8_t priority;               // Sort order (lower = higher in list)
    ui::IView* (*getView)();        // Factory function to get the view (push view on select)
    bool (*isVisible)();            // Optional visibility check (nullptr = always visible)
    const char* moduleName;         // Owner module name (set automatically)
    MenuLocation location;          // Where to show this item
    void (*onSelect)();             // Toggle/action callback (used when getView is nullptr)
};
```

Usage in `ModuleRegistry.cpp:217-241`:
```cpp
uint8_t ModuleRegistry::getMenuItems(MenuLocation location, ModuleMenuItem* items, uint8_t maxItems) {
    // ...
    for (uint8_t j = 0; j < count && totalCount < maxItems; j++) {
        if (moduleItems[j].location == location) {
            // Check visibility
            if (moduleItems[j].isVisible && !moduleItems[j].isVisible()) {
                continue;
            }
            // Set module name
            moduleItems[j].moduleName = modules_[i]->getName();
            items[totalCount++] = moduleItems[j];
        }
    }
    // ...
}
```

The `isVisible` function pointer:
- Is optional (nullptr means always visible)
- Requires callers to provide a function even for simple cases
- Adds indirection for a simple boolean check

## Impact
- **Unnecessary complexity**: Most items don't need dynamic visibility
- **Boilerplate**: Requires defining a static function for simple visibility checks
- **Indirection**: Harder to trace visibility logic

## Recommended Fix
Use a simpler approach:

```cpp
// Option 1: Use a simple flag for common cases
struct ModuleMenuItem {
    const char* label;
    uint8_t priority;
    ui::IView* (*getView)();
    bool alwaysVisible = true;      // Simple flag for most cases
    const char* moduleName;
    MenuLocation location;
    void (*onSelect)();
};

// Option 2: Use std::function for flexibility (if C++17 available)
#include <functional>

struct ModuleMenuItem {
    const char* label;
    uint8_t priority;
    ui::IView* (*getView)();
    std::function<bool()> isVisible; // nullptr/empty = always visible
    const char* moduleName;
    MenuLocation location;
    void (*onSelect)();
};

// Usage with lambda:
ModuleMenuItem item{
    .label = "Settings",
    .priority = 1,
    .getView = []() { return &settingsView; },
    .isVisible = []() { return isUserLoggedIn(); },
    .location = MenuLocation::MAIN_MENU
};
```

Or keep it simpler:
```cpp
// Option 3: Just remove it, handle visibility at registration time
struct ModuleMenuItem {
    const char* label;
    uint8_t priority;
    ui::IView* (*getView)();
    const char* moduleName;
    MenuLocation location;
    void (*onSelect)();
};

// Modules check visibility themselves before registering items
```

## References
- Martin Fowler, "Refactoring: Improving the Design of Existing Code" - Speculative Generality
- YAGNI (You Ain't Gonna Need It) - Add only what is needed now
