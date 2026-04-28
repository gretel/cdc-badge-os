---
title: "[LOW] Delete Actions in Context Menus Lack Visual Distinction"
severity: LOW
domain: destructive-actions
lens: ui-delete-flows
labels:
  - audit:ux-antipatterns/destructive-actions
---

## Summary
Delete actions in context menus are presented with the same visual styling as safe actions (View, Edit). The `ContextMenuView` component does not provide a way to mark actions as "dangerous" with distinct styling (e.g., red color, warning icon, bold text).

**Evidence:**
- File: `components/mod_password/src/PasswordModule.cpp`
- Lines: 715-722

```cpp
static ui::ContextMenuItem items[] = {
    {mstr(STR_VIEW), onMenuView},
    {mstr(STR_EDIT), onMenuEdit},
    {mstr(STR_DELETE), onMenuDelete}  // Same styling as View/Edit!
};
ui::showContextMenu(mstr(STR_ACTIONS), items, 3);
```

- File: `components/mod_fido2/src/Fido2Ui.cpp`
- Lines: 265-275

```cpp
static ui::ContextMenuItem items[3];
items[0] = {mstr(STR_DETAILS), []() { showDetail(...) }};
items[1] = {ui::tr(ui::StringId::DELETE), []() { handleDelete(...) }};  // Same styling!
items[2] = {ui::tr(ui::StringId::CANCEL), []() {}};
showContextMenu(title, items, 3);
```

- File: `components/cdc_views/include/cdc_views/ContextMenuView.h`
- Lines: 11-13

```cpp
struct ContextMenuItem {
    const char* label;
    void (*callback)();
};
```

The `ContextMenuItem` struct has no field for styling or action type.

## Impact
- **Low Visual Friction**: Delete actions blend in with safe actions, making them easy to trigger accidentally
- **No Visual Hierarchy**: Users cannot quickly identify which action is the "risky" one
- **Consistency Issue**: Desktop/mobile apps typically use red color or warning icons for destructive menu actions

## Recommended Fix

Extend the `ContextMenuItem` struct to support an optional action type or styling flag:

```cpp
// In ContextMenuView.h
struct ContextMenuItem {
    const char* label;
    void (*callback)();
    enum class ActionType {
        NORMAL = 0,
        DELETE,       // Highlight as destructive
        WARNING,      // Warning-level action
        INFO          // Info-only action
    };
    ActionType type;  // Default: NORMAL
};

// Helper constructor for common patterns
static inline ContextMenuItem makeItem(const char* label, void (*cb)(), 
                                        ContextMenuItem::ActionType type = ContextMenuItem::ActionType::NORMAL) {
    return {label, cb, type};
}
```

Update the render function in `ContextMenuView.cpp` to apply different styling based on type:

```cpp
// In render()
for (uint8_t i = 0; i < itemCount_; i++) {
    // ... existing selection logic ...
    
    // Apply styling based on action type
    if (items_[i].type == ContextMenuItem::ActionType::DELETE) {
        // Draw in red (or bold if color not available)
        gfx->setTextColor(EPD_RED);  // Or use bold font
        gfx->print("⚠ ");  // Warning icon prefix
    } else {
        gfx->setTextColor(EPD_BLACK);
    }
    
    gfx->print(items_[i].label);
}
```

Update usage in modules:

```cpp
// In PasswordModule.cpp
static ui::ContextMenuItem items[] = {
    {mstr(STR_VIEW), onMenuView, ui::ContextMenuItem::ActionType::NORMAL},
    {mstr(STR_EDIT), onMenuEdit, ui::ContextMenuItem::ActionType::NORMAL},
    {mstr(STR_DELETE), onMenuDelete, ui::ContextMenuItem::ActionType::DELETE}
};
```

## References
- Material Design: https://m3.material.io/components/menus/guidelines (destructive actions in red)
- Apple HIG: https://developer.apple.com/design/human-interface-guidelines/menus (destructive actions get orange/red color)
