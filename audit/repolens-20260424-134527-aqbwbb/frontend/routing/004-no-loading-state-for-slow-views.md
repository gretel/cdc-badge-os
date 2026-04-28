---
title: "[LOW] No Loading State for Slow View Transitions"
severity: LOW
domain: frontend/routing
lens: routing
labels:
  - "audit:frontend/routing"
---

## Summary
When navigating to views that require data loading (e.g., TOTP list, password list, GPG key list), there is no loading indicator shown while the data is being fetched. The user may see a blank or stale view while the list is being rebuilt.

**Files affected:**
- `components/mod_totp/src/TotpModule.cpp:680-720` (rebuildList)
- `components/mod_password/src/PasswordModule.cpp` (similar pattern)
- `components/mod_gpg/src/GpgModule.cpp` (similar pattern)

## Impact
User experience issues:
1. View is pushed to stack immediately, but data may not be ready
2. User sees blank list or old data while `rebuildList()` iterates through storage
3. No visual feedback that work is in progress
4. On slower storage (or many items), this delay is noticeable

## Evidence
From `TotpModule.cpp:730-735`:
```cpp
static void onListSelect(uint16_t index, void* userData) {
    (void)userData;
    if (index == 0) {
        wizardStart();
        return;
    }
    // ...
    s_codeView.init(slot, name);
    ui::ViewStack::instance().push(&s_codeView);  // Pushed immediately
}
```

From `TotpModule.cpp:680-720` (rebuildList):
```cpp
static void rebuildList() {
    if (!ensureListBuffers()) {
        cdc::core::ModuleRegistry::instance().reportModuleError(...);
        return;
    }
    s_accountCount = 0;
    // ... iteration through storage ...
    cdc::core::TropicStorage::instance().forEachSlot(...);
    // Takes time if many entries
}
```

The view is pushed immediately after `rebuildList()`, but if storage is slow, the user sees an incomplete list.

## Recommended Fix
Add a simple loading indicator before pushing the view:

**Option 1: Show loading toast before push**
```cpp
static void onListSelect(uint16_t index, void* userData) {
    if (index == 0) {
        wizardStart();
        return;
    }
    // Show loading indicator
    ui::showToastInfo("Loading...", 1000);
    s_codeView.init(slot, name);
    ui::ViewStack::instance().push(&s_codeView);
}
```

**Option 2: Use a loading view**
Create a simple `LoadingView` that shows a spinner/text:
```cpp
void showLoadingView(const char* message) {
    static LoadingView loading;
    loading.init(message);
    ViewStack::instance().push(&loading);
}

// Then in onListSelect:
showLoadingView("Loading...");
// After data is ready:
ViewStack::instance().pop();  // Remove loading
ViewStack::instance().push(&s_codeView);
```

**Option 3: Pre-load data in onEnter**
Instead of loading in the select callback, load in `onEnter()` of the target view so it's ready when pushed.

## References
- ToastView provides `showToastInfo()` for quick loading feedback
- Consider adding a dedicated `LoadingView` component for consistency
