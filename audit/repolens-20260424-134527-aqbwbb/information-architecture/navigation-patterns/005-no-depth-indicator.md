---
title: "[LOW] No visual indication of navigation depth in footer"
severity: LOW
domain: information-architecture/navigation-patterns
lens: navigation-patterns
labels:
  - "audit:information-architecture/navigation-patterns"
---

## Summary
The footer hints show available key actions (e.g., "[Y] OK [N] Back") but do not indicate navigation depth or hierarchy level. Users cannot tell at a glance how deep they are in the navigation tree or whether they can navigate further down.

**Files affected:**
- `components/cdc_views/include/cdc_views/ListView.h` - Footer hint method (line 136)
- `components/cdc_os_ui/src/AppUi.cpp` - Footer hints for menus

## Impact
Users lack context about:
1. How deep they are in the navigation hierarchy
2. Whether they are at the root level or a submenu
3. How many 'Back' presses to reach main menu

This is particularly relevant for:
- Deep module navigation (e.g., TOTP → Account → Edit)
- Expert menu with nested module views
- Multi-step wizards (Password add/edit wizard has 5-6 steps)

## Evidence
Footer hints are static and do not reflect navigation depth:

```cpp
// components/cdc_views/include/cdc_views/ListView.h (line 136)
const char* getFooterHint() const override;
```

In ListView.cpp, footer shows only key hints:

```cpp
// components/cdc_views/src/ListView.cpp
const char* ListView::getFooterHint() const {
    if (customHint_) return customHint_;
    return tr(StringId::HINT_LIST_MENU);  // Static: "[Y] View [3] Menu [N] Back"
}
```

No integration with ViewStack depth:

```cpp
// components/cdc_ui/include/cdc_ui/ViewStack.h (line 57)
uint8_t depth() const { return depth_; }
```

The depth is available but not exposed to views for display.

Example static hints:
- `HINT_LIST_MENU` = "[Y] View  [3] Menu  [N] Back"
- `HINT_BACK` = "[N] Back"
- `HINT_APPROVE_DENY` = "[Y] Approve  [N] Deny"

None indicate "Level 3 of 5" or similar depth context.

## Recommended Fix
Add depth indicator to footer:

1. **Add depth to getFooterHint** in ListView:
   ```cpp
   const char* ListView::getFooterHint() const {
       static char hint[64];
       const char* baseHint = customHint_ ? customHint_ : tr(StringId::HINT_LIST_MENU);
       uint8_t depth = ViewStack::instance().depth();
       
       if (depth > 2) {  // Only show if deeper than main menu
           snprintf(hint, sizeof(hint), "%s  (Level %d)", baseHint, depth);
       } else {
           strcpy(hint, baseHint);
       }
       return hint;
   }
   ```

2. **Alternative: Show breadcrumb-style depth**:
   ```cpp
   // "[Main > Tools > Expert] [N] Back"
   ```

3. **Or use visual indicator**:
   ```cpp
   // "[Y] OK [N] Back  >>> Level 3 <<<"
   ```

Scope: ~30 minutes to add depth indicator to footer.

## References
- Footer hint interface: `components/cdc_views/include/cdc_views/ListView.h` line 136
- ViewStack depth: `components/cdc_ui/include/cdc_ui/ViewStack.h` line 57
- Current hints: `components/cdc_ui/include/cdc_ui/I18n.h` (string IDs)
