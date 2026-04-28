---
title: "[MEDIUM] Modal and Navigation Stack Inconsistency"
severity: MEDIUM
domain: frontend/routing
lens: routing
labels:
  - "audit:frontend/routing"
---

## Summary
The ViewStack navigation system has inconsistent handling between modal overlays and the view stack, which can lead to navigation state confusion. Specifically:

1. **Modal does not appear in stack depth**: Modals are shown/hidden via `showModal()`/`hideModal()` but are NOT counted in the stack depth. This means `depth() > 1` checks don't account for modals.

2. **Long-press N behavior ambiguity**: When a modal is showing, long-press N hides the modal. But if a modal is shown on top of a deep stack, the user might expect back navigation to work differently.

**Files affected:**
- `components/cdc_ui/src/ViewStack.cpp:190-205` (dispatchLongPress)
- `components/cdc_ui/src/ViewStack.cpp:286-313` (showModal/hideModal)
- `components/cdc_os_ui/src/AppUi.cpp:304-311` (onInactivityTimeout)

## Impact
Users may experience confusion when navigating:
- Modals don't contribute to stack depth, so `popToRoot()` or depth-based navigation doesn't account for them
- Inactivity timeout only triggers when `depth > 1`, but modals can be shown at any depth
- If a modal is displayed while navigating deep into menus, the navigation state becomes harder to reason about

## Evidence
From `ViewStack.cpp:190-205`:
```cpp
void ViewStack::dispatchLongPress(char key) {
    // Long-press N always goes back (universal behavior)
    if (key == 'N') {
        if (modal_) {
            hideModal();  // Modal takes priority
        } else if (depth_ > 1) {
            pop();
        }
        return;
    }
    // ...
}
```

From `AppUi.cpp:304-311`:
```cpp
static void onInactivityTimeout() {
    // Lock screen
    while (ViewStack::instance().depth() > 1) {
        ViewStack::instance().pop();
    }
    s_ignoreKeyUntilRelease = true;
    clearKeypadBuffer();
}
```

Note: `depth_` only counts stack items, not modals. A modal shown at depth 1 still has `depth() == 1`.

## Recommended Fix
Choose one of these approaches for consistency:

**Option A (Simple):** Document that modals are transient and don't affect navigation state. Ensure all timeout/depth logic explicitly handles modals:
```cpp
// In onInactivityTimeout, also hide modal first
if (modal_) {
    hideModal();
}
while (depth() > 1) {
    pop();
}
```

**Option B (Integrated):** Count modals in depth (requires more changes):
- Add `modalDepth_` counter or include modal in stack
- Update all `depth()` checks to account for modal

**Option C (Explicit):** Add methods like `popAllIncludingModal()` for clear intent:
```cpp
void popAllIncludingModal() {
    if (modal_) hideModal();
    popToRoot();
}
```

## References
- ViewStack header: `components/cdc_ui/include/cdc_ui/ViewStack.h`
- ToastView usage: `components/cdc_views/src/ToastView.cpp:178`
