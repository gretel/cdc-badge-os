---
title: "[MEDIUM] Inconsistent boolean member variable naming in cdc_views and cdc_core"
severity: MEDIUM
domain: code-quality/naming
lens: naming-conventions
labels:
  - "audit:code-quality/naming"
---

## Summary

Boolean member variables in CDC Badge OS core components follow **inconsistent naming conventions**. The codebase uses:

1. **Underscore suffix style**: `dirty_`, `title_`, `selection_`, `scrollPos_`, `onSelect_`, `itemRenderer_`, `preservePosition_`
2. **Mixed styles in same struct**: `iconDisabled`, `userData` (no prefix) alongside underscore-suffixed members

### Affected Files

| File | Inconsistent Members |
|------|---------------------|
| `components/cdc_views/include/cdc_views/ListView.h:17` | `iconDisabled`, `userData` (no prefix) |
| `components/cdc_views/include/cdc_views/ListView.h:133-140` | `title_`, `customHint_`, `items_`, `itemCount_`, `selection_`, `scrollPos_`, `onSelect_`, `onMenu_`, `itemRenderer_`, `itemRendererCtx_`, `preservePosition_`, `itemHeight_`, `visibleItems_` (all underscore suffix) |
| `components/cdc_ui/include/cdc_ui/IView.h:138-139` | `dirty_`, `title_` (underscore suffix) |
| `components/cdc_core/include/cdc_core/EventBus.h:141` | `handlers_`, `initialized_` (underscore suffix) |
| `components/cdc_core/include/cdc_core/ServiceRegistry.h:135-137` | `services_`, `count_`, `typedServices_` (underscore suffix) |

## Impact

**Maintainability burden**: Developers adding new members to `ListItem` struct must guess whether to use underscore suffix or not.

**Inconsistency within structs**: `ListItem` struct has `iconDisabled`, `userData` (no prefix) while `ListView` class uses underscore suffix for all members.

**Readability**: The underscore suffix convention is clear, but the `ListItem` struct breaks this pattern without justification.

## Evidence

From `components/cdc_views/include/cdc_views/ListView.h:13-18`:
```cpp
struct ListItem {
    const char* label;          // Display text
    uint8_t icon = 0;           // Icon type (0 = none)
    bool iconDisabled = false;  // Draw icon crossed-out (no underscore suffix)
    void* userData = nullptr;   // Optional user data (no underscore suffix)
};
```

From `components/cdc_views/include/cdc_views/ListView.h:132-148`:
```cpp
private:
    const char* title_ = nullptr;           // underscore suffix
    const char* customHint_ = nullptr;      // underscore suffix
    const ListItem* items_ = nullptr;       // underscore suffix
    uint16_t itemCount_ = 0;                // underscore suffix
    uint16_t selection_ = 0;                // underscore suffix
    uint16_t scrollPos_ = 0;                // underscore suffix
    SelectCallback onSelect_ = nullptr;     // underscore suffix
    MenuCallback onMenu_ = nullptr;         // underscore suffix
    ItemRenderCallback itemRenderer_ = nullptr;  // underscore suffix
    void* itemRendererCtx_ = nullptr;       // underscore suffix
    bool preservePosition_ = false;         // underscore suffix
    uint8_t itemHeight_ = DEFAULT_ITEM_HEIGHT;   // underscore suffix
    uint8_t visibleItems_ = 4;              // underscore suffix
```

From `components/cdc_ui/include/cdc_ui/IView.h:138-139`:
```cpp
protected:
    bool dirty_ = true;
    const char* title_ = nullptr;
```

## Recommended Fix

**Update `ListItem` struct** to use consistent underscore suffix convention:

```cpp
struct ListItem {
    const char* label;          // Display text
    uint8_t icon = 0;           // Icon type (0 = none)
    bool iconDisabled_ = false; // Draw icon crossed-out
    void* userData_ = nullptr;  // Optional user data
};
```

Then update all usages of `iconDisabled` and `userData` throughout the codebase:
- `components/cdc_views/src/ListView.cpp`
- Any module code that uses `ListItem`

Process:
1. Rename `iconDisabled` to `iconDisabled_` in `ListView.h`
2. Rename `userData` to `userData_` in `ListView.h`
3. Find and replace all usages: `grep -r "iconDisabled" --include="*.cpp" --include="*.h"`
4. Find and replace all usages: `grep -r "->userData"` and `grep -r "\.userData"`
5. Verify compilation and test

## References

- [Google C++ Style Guide - Naming](https://google.github.io/styleguide/cppguide.html#Naming)
- [Clean Code: Member variable naming](https://cleancode.uservoice.com/forums/178663-clean-code-articles/suggestions/4658878-member-variables-should-have-trailing-underscore)

</content>