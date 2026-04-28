---
title: "[MEDIUM] Toast notifications can stack without queue management"
severity: MEDIUM
domain: UI/UX
lens: cognitive-overload
labels:
  - toast-stacking
  - modal-overlays
---

## Summary

The toast notification system (`ToastView`) uses a single shared instance (`s_sharedToast`) displayed as a modal overlay. When multiple toasts are triggered in quick succession (e.g., during Wi-Fi setup with multiple error conditions), they can accumulate on the view stack without a proper queue mechanism, leading to overlapping or stacked modals.

**Files:**
- `components/cdc_views/src/ToastView.cpp:175-248` (toast implementation)
- `components/cdc_ui/src/ViewStack.cpp:280-315` (modal handling)
- `components/cdc_os_ui/src/WifiMenuUi.cpp:576-611` (multiple sequential toasts in Wi-Fi setup)

## Impact

**User Experience:** Multiple toast notifications can overlap or stack, creating visual clutter and confusion. Users may miss important messages if they are obscured by subsequent toasts.

**Evidence:**
In `WifiMenuUi.cpp` lines 576-611, the static IP validation flow can trigger multiple error toasts in sequence:
```cpp
// Line 576: Invalid IP toast
showToastError("Invalid IP", TOAST_DURATION_MEDIUM_MS);
wifiShowIpInputField(...);

// Line 591: Another Invalid IP toast if gateway is wrong
showToastError("Invalid IP", TOAST_DURATION_MEDIUM_MS);

// Line 606: Another Invalid IP toast if netmask is wrong
showToastError("Invalid IP", TOAST_DURATION_MEDIUM_MS);
```

The `showToastInternal` function (ToastView.cpp:175) always calls `ViewStack::instance().showModal()`, but if a modal is already present, it simply replaces it without queueing.

## Recommended Fix

Implement a simple toast queue with the following behavior:

1. Add a queue data structure (e.g., `std::deque<std::function<void()>>`) to store pending toasts
2. Modify `showToastInternal` to add toasts to the queue instead of immediately showing them
3. When a toast expires or is dismissed, pop the next toast from the queue
4. Limit queue depth to 3-5 toasts to prevent memory buildup

Alternative simpler fix:
- Add a check in `showModal()` to dismiss any existing toast before showing a new one
- Ensure `hideModal()` triggers the next toast in a simple queue

## References

- Nielsen Norman Group: "Notification Design Best Practices"
- Material Design: "Snackbar & Toast" guidelines (queueing behavior)
