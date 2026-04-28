---
title: "[MEDIUM] Inconsistent back-navigation behavior across views"
severity: MEDIUM
domain: navigation-patterns
lens: information-architecture
labels:
  - "audit:information-architecture/navigation-patterns"
---

## Summary
The navigation stack uses `ViewStack::instance()` for hierarchical view navigation, but back-navigation behavior is inconsistent across modules. Some views use `N` key for back navigation with `InputResult::REQUEST_POP`, while others manually pop the stack with `while (ViewStack::instance().depth() > 1) { ViewStack::instance().pop(); }`.

**Evidence locations:**
- `components/mod_totp/src/TotpModule.cpp:280` - Uses `InputResult::REQUEST_POP` for 'N' key
- `components/mod_totp/src/TotpModule.cpp:424` - Manual stack popping: `while (ui::ViewStack::instance().depth() > 1)`
- `components/mod_gpg/src/GpgModule.cpp:285` - Manual stack popping pattern
- `components/cdc_os_ui/src/AppUi.cpp:300` - Manual stack popping for inactivity timeout

## Impact
**Maintenance burden:** Developers must remember which pattern to use when creating new views.
**User experience inconsistency:** Some views may behave differently when navigating back (e.g., whether to pop all the way to root or just one level).
**Bug risk:** Manual stack manipulation can lead to navigation stack corruption if not done carefully.

## Evidence
```cpp
// Inconsistent pattern 1: REQUEST_POP (correct)
ui::InputResult onKey(char key) override {
    if (key == 'N') {
        return ui::InputResult::REQUEST_POP;  // TotpCodeView
    }
}

// Inconsistent pattern 2: Manual pop loop
while (ui::ViewStack::instance().current() != &s_listView &&
       ui::ViewStack::instance().depth() > 1) {
    ui::ViewStack::instance().pop();  // wizardFinish()
}

// Inconsistent pattern 3: Direct pop
ViewStack::instance().pop();  // NvsEditModule.cpp
```

## Recommended Fix
Establish a consistent back-navigation pattern:
1. **Standardize on `InputResult::REQUEST_POP`** for single-step back navigation
2. **Create a helper function** `popToView(IView* target)` for multi-level navigation
3. **Document the pattern** in `IView.h` or a navigation guide
4. **Update existing code** to use the standardized patterns

Example helper:
```cpp
static void popToRoot() {
    while (ViewStack::instance().depth() > 1) {
        ViewStack::instance().pop();
    }
}
```

## References
- `components/cdc_ui/include/cdc_ui/IView.h` - InputResult enum definition
- `components/cdc_ui/include/cdc_ui/ViewStack.h` - Navigation stack API
