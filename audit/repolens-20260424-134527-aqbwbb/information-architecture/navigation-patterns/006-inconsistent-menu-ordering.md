---
title: "[LOW] Inconsistent menu ordering across different menu locations"
severity: LOW
domain: navigation-patterns
lens: information-architecture
labels:
  - "audit:information-architecture/navigation-patterns"
---

## Summary
Module menu items use a `priority` field for sorting, but the same module can register items in different menu locations (MAIN_MENU, TOOLS_MENU, EXPERT_MENU) with potentially conflicting or inconsistent priority values. This can lead to confusing menu ordering where related items are scattered across menus without a clear pattern.

**Evidence locations:**
- `components/cdc_core/include/cdc_core/IModule.h:28` - `uint8_t priority;` field
- `components/mod_totp/src/TotpModule.cpp:550` - Priority 50 for MAIN_MENU
- `components/cdc_core/src/ModuleRegistry.cpp:240` - Bubble sort by priority

## Impact
**User confusion:** Related items may appear in different positions across menus.
**Inconsistent UX:** Modules may have different ordering patterns (alphabetical vs. frequency-based).
**Maintenance difficulty:** No clear convention for what priority values mean.

## Evidence
```cpp
// In IModule.h - priority is just a number, no documentation
struct ModuleMenuItem {
    const char* label;
    uint8_t priority;  // What do these values mean?
    ui::IView* (*getView)();
    MenuLocation location;
};

// In TotpModule.cpp - priority 50
items[0] = {mstr(STR_TOTP), 50, ...};  // What does 50 mean?

// In ModuleRegistry.cpp - simple bubble sort
for (uint8_t i = 0; i < totalCount; i++) {
    for (uint8_t j = i + 1; j < totalCount; j++) {
        if (items[j].priority < items[i].priority) {
            ModuleMenuItem tmp = items[i];
            items[i] = items[j];
            items[j] = tmp;
        }
    }
}
```

## Recommended Fix
Establish priority conventions:
1. **Document priority ranges** (e.g., 0-20: fixed items, 21-50: common modules, 51-100: less common)
2. **Enforce consistent ordering** within each menu location
3. **Add default ordering** for modules without explicit priority

Example:
```cpp
// Documented priority convention
enum MenuPriority {
    PRIORITY_FIXED = 0,       // Fixed items like Tools, Settings
    PRIORITY_COMMON = 20,     // Common modules (FIDO2, TOTP, Password)
    PRIORITY_STANDARD = 50,   // Standard modules
    PRIORITY_ADVANCED = 80,   // Advanced/less-used modules
    PRIORITY_EXPERT = 100     // Expert-only items
};

// Usage
items[0] = {mstr(STR_TOTP), PRIORITY_STANDARD, ...};
```

## References
- `components/cdc_core/include/cdc_core/IModule.h` - ModuleMenuItem structure
- `components/cdc_core/src/ModuleRegistry.cpp` - Sorting implementation
