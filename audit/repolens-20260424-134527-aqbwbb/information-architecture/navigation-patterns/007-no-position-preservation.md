---
title: "[MEDIUM] No way to return to previously viewed item in list"
severity: MEDIUM
domain: navigation-patterns
lens: information-architecture
labels:
  - "audit:information-architecture/navigation-patterns"
---

## Summary
When a user selects an item from a list (e.g., TOTP account list), navigates to a detail view, and returns, the list doesn't preserve the previous selection position. Users must scroll back to find the item they were just viewing.

**Evidence locations:**
- `components/cdc_views/include/cdc_views/ListView.h:95` - `preservePosition()` method exists but not consistently used
- `components/mod_totp/src/TotpModule.cpp:424` - Returns to list without preserving position
- `components/cdc_os_ui/src/ExpertMenuUi.cpp:105` - Module list doesn't preserve position

## Impact
**User frustration:** Users must manually find their place after returning from detail views.
**Inefficiency:** Especially problematic for long lists (e.g., password vault with 353 entries).
**Accessibility:** Users with motor difficulties must scroll repeatedly.

## Evidence
```cpp
// ListView has preservePosition() but it's not used consistently
void ListView::preservePosition() { 
    preservePosition_ = true; 
}

// In TotpModule.cpp - no position preservation
static void onListSelect(uint16_t index, void* userData) {
    if (index == 0) {
        wizardStart();
        return;
    }
    // ...
    s_codeView.init(slot, name);
    ui::ViewStack::instance().push(&s_codeView);  // No preservePosition() call
}

// In wizardFinish() - returns without preserving
static void wizardFinish() {
    // ...
    rebuildList();
    while (ui::ViewStack::instance().depth() > 1) {
        ui::ViewStack::instance().pop();  // Position lost!
    }
}
```

## Recommended Fix
Automatically preserve list position:
1. **Call `preservePosition()`** before pushing detail view
2. **Store selection index** in view context
3. **Restore position** in `onResume()`

Example:
```cpp
// In onListSelect
static void onListSelect(uint16_t index, void* userData) {
    s_listView.preservePosition();  // Remember position
    s_codeView.init(slot, name);
    ui::ViewStack::instance().push(&s_codeView);
}

// In wizardFinish()
static void wizardFinish() {
    // ...
    rebuildList();
    s_listView.setSelection(savedIndex);  // Restore position
    while (ui::ViewStack::instance().depth() > 1) {
        ui::ViewStack::instance().pop();
    }
}
```

## References
- `components/cdc_views/include/cdc_views/ListView.h` - preservePosition() method
- `components/mod_totp/src/TotpModule.cpp` - List navigation
