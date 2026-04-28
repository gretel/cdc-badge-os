---
title: "[MEDIUM] Menus not rebuilt when returning from module views"
severity: MEDIUM
domain: navigation-patterns
lens: information-architecture
labels:
  - "audit:information-architecture/navigation-patterns"
---

## Summary
When a module view is pushed onto the navigation stack (e.g., TOTP list view), the menu items are not refreshed when the user returns. This means any state changes made in the module (e.g., adding/deleting TOTP accounts) are not reflected in the menu until the menu is manually accessed again.

**Evidence locations:**
- `components/mod_totp/src/TotpModule.cpp:450` - `rebuildList()` called only when entering view
- `components/cdc_os_ui/src/AppUi.cpp:393` - No menu rebuild after popping view
- `components/mod_gpg/src/GpgModule.cpp:285` - Similar pattern, no refresh on return

## Impact
**Stale data:** Users may see outdated menu items after making changes.
**Confusion:** Users might think their changes didn't save because the UI doesn't update.
**Extra navigation:** Users must exit and re-enter menus to see updates.

## Evidence
```cpp
// In TotpModule.cpp - rebuildList() only called on entry
static void onListSelect(uint16_t index, void* userData) {
    if (index == 0) {
        wizardStart();
        return;
    }
    // ...
    s_codeView.init(slot, name);
    ui::ViewStack::instance().push(&s_codeView);  // No rebuild on return
}

// In wizardFinish() - only rebuilds local list, not parent menus
static void wizardFinish() {
    // ...
    rebuildList();  // Only rebuilds TOTP list
    while (ui::ViewStack::instance().depth() > 1) {
        ui::ViewStack::instance().pop();
    }
    // Main menu not refreshed!
}
```

## Recommended Fix
Implement automatic menu refresh on view return:
1. **Override `onResume()`** in views to refresh data
2. **Add callback** to notify parent menu when child view changes data
3. **Use EventBus** to publish "view changed" events

Example:
```cpp
void TotpCodeView::onResume() {
    // Find and refresh parent list view
    auto* parent = dynamic_cast<ListView*>(ViewStack::instance().at(
        ViewStack::instance().depth() - 1
    ));
    if (parent) rebuildList();
}
```

## References
- `components/cdc_ui/include/cdc_ui/IView.h` - View lifecycle methods
- `components/cdc_core/include/cdc_core/EventBus.h` - Event system for notifications
