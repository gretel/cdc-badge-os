---
title: "[MEDIUM] Sticky toast modal has no dismissal mechanism"
severity: MEDIUM
domain: UI/UX - Modal Dialogs
lens: flow-dead-ends
labels:
  - "audit:ux-antipatterns/flow-dead-ends"
---

## Summary
The `showToastAlertSticky()` function displays a non-dismissible toast message that **cannot be dismissed by any action** and **does not auto-dismiss**. This creates a modal overlay that traps the user until the device is reset or the view stack changes.

**Location:** `components/cdc_views/src/ToastView.cpp`, lines 247-249

**Usage locations:**
- `components/cdc_os_ui/src/AppUi.cpp:552` - Shows "Slot map invalid" error
- `components/cdc_os_ui/src/ExpertMenuUi.cpp:110` - Shows "USB replug required"

## Impact
When a sticky toast is shown:
1. The toast is displayed as a modal overlay over the current view
2. It does **not auto-dismiss** (duration = 0)
3. It does **not respond to key presses** (dismissible = false)
4. The user is **trapped** looking at the message with no way to dismiss it

This is particularly problematic for:
- **Error states**: "Slot map invalid" - user needs to know the error but also needs to continue using the device
- **Status messages**: "USB replug required" - user needs to act on the message but the modal blocks interaction

The toast modal sits on top of the view stack, blocking interaction with the underlying view. Users cannot:
- Press any key to dismiss it (keys are ignored)
- Wait for it to auto-dismiss (timeout is 0)
- Access the underlying view's functionality

## Evidence
**showToastAlertSticky implementation (lines 247-249):**

```cpp
void showToastAlertSticky(const char* message) {
    showToastInternal(message, ToastView::Icon::ALERT, 0, false);
}
```

Passes `durationMs = 0` and `dismissible = false`.

**ToastView `onTick` method (lines 44-49):**

```cpp
void ToastView::onTick(uint32_t nowMs) {
    if (durationMs_ > 0 && !expired_) {
        if (nowMs - startMs_ >= durationMs_) {
            expired_ = true;
            ViewStack::instance().hideModal();
        }
    }
}
```

With `durationMs_ = 0`, the auto-dismiss logic is skipped entirely.

**ToastView `onKey` method (lines 55-65):**

```cpp
InputResult ToastView::onKey(char key) {
    // Any Y or N key dismisses the toast (if dismissible)
    if (dismissible_ && (key == 'Y' || key == 'N')) {
        expired_ = true;
        ViewStack::instance().hideModal();
        return InputResult::CONSUMED;
    }
    return InputResult::IGNORED;
}
```

With `dismissible_ = false`, key presses are ignored.

**Usage in AppUi.cpp (line 552):**

```cpp
if (!core::TropicSlotMap::instance().isValid()) {
    const char* msg = core::TropicSlotMap::instance().errorMessage();
    showToastAlertSticky(msg ? msg : "Slot map invalid");
}
```

This shows a sticky toast on the lock screen when the slot map is invalid. The user sees the error but cannot dismiss the toast to continue using the device.

## Recommended Fix
Add a dismissal mechanism for sticky toasts. Options:

**Option 1: Make sticky toasts dismissible with any key**
Modify `ToastView::onKey()` to always allow dismissal, regardless of `dismissible_` flag:

```cpp
InputResult ToastView::onKey(char key) {
    // Any Y or N key dismisses the toast
    if (key == 'Y' || key == 'N') {
        // If not dismissible, only allow when duration is 0 (sticky)
        // This prevents accidental dismissal of transient toasts
        if (dismissible_ || durationMs_ == 0) {
            expired_ = true;
            ViewStack::instance().hideModal();
            return InputResult::CONSUMED;
        }
    }
    return InputResult::IGNORED;
}
```

**Option 2: Use a short timeout instead of truly sticky**
Change `showToastAlertSticky()` to use a long but finite timeout:

```cpp
void showToastAlertSticky(const char* message) {
    showToastInternal(message, ToastView::Icon::ALERT, 5000, true);  // 5 seconds, dismissible
}
```

**Option 3: Add explicit footer hint for sticky toasts**
Show a hint that the user can press a key to dismiss:

```cpp
void ToastView::render(bool partial) {
    // ... existing render code ...
    
    // Add dismiss hint for sticky toasts
    if (durationMs_ == 0) {
        gfx->setCursor(boxX + 10, boxY + BOX_HEIGHT + 5);
        gfx->print("[Y/N] Dismiss");
    }
}
```

**Option 4: Use a different UI pattern for persistent errors**
Instead of a sticky toast, use an InfoView or MessageBox that clearly supports navigation:

```cpp
// For slot map errors, push an info view instead
if (!core::TropicSlotMap::instance().isValid()) {
    const char* msg = core::TropicSlotMap::instance().errorMessage();
    showInfo("Error", msg, "[N] Back");
}
```

## References
- **User Flow Dead Ends:** Modal overlays without exit mechanism
- **Conditional Rendering That Removes All Navigation:** "Full-screen modal flows that remove the underlying page navigation without providing their own close or exit mechanism"
- **ToastView API:** `components/cdc_views/include/cdc_views/ToastView.h`
