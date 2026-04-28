---
title: "[HIGH] Non-dismissible alert toasts lack user-controlled dismissal mechanism"
severity: HIGH
domain: notification-interrupts
lens: notification-interrupts
labels:
  - "toast-implementation"
  - "user-control"
---

## Summary
The `showToastAlertSticky()` function creates non-dismissible toast alerts that cannot be dismissed by the user. These toasts are used in critical paths like slot map validation and USB replug warnings.

**Files affected:**
- `components/cdc_views/src/ToastView.cpp:247-249`
- `components/cdc_views/include/cdc_views/ToastView.h:103`
- `components/cdc_os_ui/src/AppUi.cpp:552`
- `components/cdc_os_ui/src/ExpertMenuUi.cpp:110`

**Evidence:**
```cpp
// ToastView.cpp:247-249
void showToastAlertSticky(const char* message) {
    showToastInternal(message, ToastView::Icon::ALERT, 0, false);
}

// AppUi.cpp:552 - Called on startup if slot map is invalid
if (!core::TropicSlotMap::instance().isValid()) {
    const char* msg = core::TropicSlotMap::instance().errorMessage();
    showToastAlertSticky(msg ? msg : "Slot map invalid");
}

// ExpertMenuUi.cpp:110 - USB replug warning
if (!needsReplugBefore && needsReplugAfter) {
    showToastAlertSticky(tr(StringId::USB_REPLUG_REQUIRED));
}
```

The `dismissible_` flag is set to `false`, and the `onKey()` handler only dismisses if `dismissible_` is true:
```cpp
// ToastView.cpp:60-67
InputResult ToastView::onKey(char key) {
    if (dismissible_ && (key == 'Y' || key == 'N')) {
        expired_ = true;
        ViewStack::instance().hideModal();
        return InputResult::CONSUMED;
    }
    return InputResult::IGNORED;
}
```

## Impact
- **User frustration**: Users cannot dismiss critical alerts even after acknowledging them
- **Blocking workflow**: Non-dismissible toasts block interaction until the module is toggled again or the device restarts
- **No escape hatch**: For sticky alerts, there is no clear path to dismiss them except navigating away

## Evidence
1. `showToastAlertSticky()` passes `dismissible = false` to `showToastInternal()`
2. `onKey()` only responds to Y/N keys if `dismissible_` is true
3. `onTick()` only auto-dismisses if `durationMs_ > 0`, but sticky alerts use `durationMs = 0`
4. Called in `AppUi.cpp:552` for slot map errors and `ExpertMenuUi.cpp:110` for USB replug warnings

## Recommended Fix
Add a dismiss mechanism for sticky alerts:

1. **Option A (Recommended)**: Allow any key to dismiss sticky alerts (not just Y/N):
```cpp
InputResult ToastView::onKey(char key) {
    // Non-dismissible toasts can still be dismissed with any key
    if (dismissible_ && (key == 'Y' || key == 'N')) {
        expired_ = true;
        ViewStack::instance().hideModal();
        return InputResult::CONSUMED;
    }
    // Allow any key to dismiss sticky alerts after user acknowledges
    if (!dismissible_) {
        expired_ = true;
        ViewStack::instance().hideModal();
        return InputResult::CONSUMED;
    }
    return InputResult::IGNORED;
}
```

2. **Option B**: Add a timeout for sticky alerts (e.g., 5 seconds minimum):
```cpp
void showToastAlertSticky(const char* message) {
    showToastInternal(message, ToastView::Icon::ALERT, 5000, true);  // 5s, dismissible
}
```

## References
- UX Best Practices: Alerts should always have a way to be dismissed
- WCAG 2.1: Users should be able to control timing of information updates
