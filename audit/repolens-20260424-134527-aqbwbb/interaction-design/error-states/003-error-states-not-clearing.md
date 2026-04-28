---
title: "[MEDIUM] Error states persist after navigation and retry operations"
severity: MEDIUM
domain: interaction-design/error-states
lens: error-state-clearing
labels:
  - "audit:interaction-design/error-states"
---

## Summary
Error toasts and messages are not properly cleared when users navigate between views or retry operations, leading to stale error states being displayed alongside new content.

**Key issues:**
1. ToastView uses a shared static instance that may persist after navigation
2. No explicit error state clearing in view `onEnter()`/`onResume()` callbacks
3. MessageBox overlay doesn't clear when user navigates away

## Impact
**Confusing UX:** Users see error messages from previous screens overlaid on new content.

**Stale data display:** Error states from failed operations may persist even after successful retry.

## Evidence
```cpp
// components/cdc_views/src/ToastView.cpp:175
static ToastView s_sharedToast;  // Shared singleton

static void showToastInternal(const char* message, ToastView::Icon icon, uint16_t durationMs,
                              bool dismissible) {
    s_sharedToast.init(message, icon, durationMs, dismissible);
    ViewStack::instance().showModal(&s_sharedToast);
    ViewStack::instance().render();
}

// Problem: If user navigates away while toast is visible, 
// the modal remains on the view stack and may reappear
```

```cpp
// components/mod_totp/src/TotpModule.cpp:360
void TotpCodeView::onEnter(void* context) {
    updateCode();
    if (!timeValid_) {
        ui::showToastError(mstr(STR_TIME_INVALID));  // Shows toast every time
    }
    // No check if toast is already visible/stale
}

// Components/cdc_os_ui/src/WifiMenuUi.cpp
void showWifiMainMenu() {
    // ...
    if (!connected) {
        showToastError(tr(StringId::WIFI_FAILED), TOAST_DURATION_LONG_MS);
    }
    // If user navigates back and forth quickly, multiple toasts may queue
}
```

**PinEntryView error accumulation:**
```cpp
// components/cdc_views/src/PinEntryView.cpp:155
void PinEntryView::verify() {
    if (!valid) {
        attempts_++;
        showMessage(tr(StringId::WRONG_PIN), MessageIcon::ERROR, 1500);
        // Error shown, but if user retries quickly, old MessageBox 
        // might still be visible
    }
}
```

## Recommended Fix
**1. Clear modal toasts on view navigation:**

```cpp
// ViewStack.cpp - in pop() and replace()
void ViewStack::pop() {
    // Clear any pending toast modals
    if (modal_ && dynamic_cast<ToastView*>(modal_)) {
        hideModal();
    }
    // ... existing pop logic
}
```

**2. Reset error state in view lifecycle callbacks:**

```cpp
// TotpCodeView example
void TotpCodeView::onEnter(void* context) {
    updateCode();
    // Clear any previous error state first
    if (ui::ViewStack::instance().getModal() && 
        dynamic_cast<ui::ToastView*>(ui::ViewStack::instance().getModal())) {
        ui::ViewStack::instance().hideModal();
    }
    
    if (!timeValid_) {
        ui::showToastError(mstr(STR_TIME_INVALID));
    }
}

void TotpCodeView::onResume() {
    // Same cleanup on resume
    if (ui::ViewStack::instance().getModal() && 
        dynamic_cast<ui::ToastView*>(ui::ViewStack::instance().getModal())) {
        ui::ViewStack::instance().hideModal();
    }
    updateCode();
    if (!timeValid_) {
        ui::showToastError(mstr(STR_TIME_INVALID));
    }
}
```

**3. Use unique toast instances per view (optional):**

Instead of a shared `s_sharedToast`, consider:
```cpp
// Each view owns its toast
class TotpCodeView {
private:
    ui::ToastView toast_;  // Instance member, not static
};
```

**4. Add explicit error clearing for PinEntryView:**

```cpp
void PinEntryView::onEnter(void* context) {
    clear();
    // Clear any lingering error messages
    ui::hideMessage();  
    dirty_ = true;
}
```

**Estimated effort:** 1 hour for ViewStack changes, 30 min for view-specific fixes

## References
- [React: Component Lifecycle](https://reactjs.org/docs/react-component.html#component-lifecycle)
- [Android: Activity Lifecycle](https://developer.android.com/guide/components/activities/activity-lifecycle)
