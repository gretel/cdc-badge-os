---
title: "[MEDIUM] No mechanism to suppress non-critical toasts during focused tasks"
severity: MEDIUM
domain: notification-interrupts
lens: notification-interrupts
labels:
  - "audit:ux-antipatterns/notification-interrupts"
---

## Summary
The notification system has no quiet mode or focus-aware suppression. Toasts can interrupt users during any context, including PIN entry, text input, or critical workflows. There's no way to temporarily suppress non-critical notifications or batch them for later display.

**Location:** `components/cdc_views/src/ToastView.cpp`, `components/cdc_ui/src/ViewStack.cpp`

## Impact
- **Context interruption**: Toasts can appear during PIN entry, date/time input, or other focused tasks
- **Input confusion**: Users may mistake notification feedback for input confirmation
- **Workflow disruption**: Critical actions (like FIDO2 approval) can be interrupted by module error toasts
- **No batching**: Multiple notifications during a task all appear immediately instead of being queued

## Evidence

**No quiet mode context** (`components/cdc_ui/src/ViewStack.cpp`):
```cpp
// ViewStack has no context tracking:
class ViewStack {
    // ...
    // No: bool quietMode_;
    // No: uint8_t suppressDepth_;  // Track nested views that should suppress toasts
    // No: std::vector<QueuedToast> notificationBuffer_;
};
```

**Toasts render over any view** (`components/cdc_views/src/ToastView.cpp:175-180`):
```cpp
static void showToastInternal(const char* message, ToastView::Icon icon, uint16_t durationMs,
                              bool dismissible = true) {
    s_sharedToast.init(message, icon, durationMs, dismissible);
    ViewStack::instance().showModal(&s_sharedToast);  // Shows over ANY view
    ViewStack::instance().render();
}
```

**PIN entry can be interrupted** (`components/cdc_os_ui/src/AppUi.cpp:276-290`):
```cpp
static void onUnlockRequested() {
    clearKeypadBuffer();
    s_ignoreKeyUntilRelease = true;
    
    if (!core::PinManager::instance().isPinSet()) {
        onPinSuccess();
        return;
    }
    
    if (s_pinEntry) {
        s_pinEntry->clear();
        ViewStack::instance().push(s_pinEntry);  // PIN entry pushed
    }
}

// But a module error could still trigger a toast:
// components/cdc_os_ui/src/ExpertMenuUi.cpp:285-301
void onModuleErrorEvent(const core::Event& evt) {
    // ...
    showToastError(errMsg, TOAST_DURATION_LONG_MS);  // Shows during PIN entry!
}
```

**No suppression during input views:**
```cpp
// PinEntryView, DateInputView, TimeInputView all lack:
// - onToastSuppress() method
// - isSilentContext() check
// - notification buffer
```

**Module error events fire regardless of context** (`components/cdc_os_ui/src/AppUi.cpp:688`):
```cpp
// Subscription in ui_init:
core::EventBus::instance().subscribe(onModuleErrorEvent, 
                                      static_cast<uint32_t>(core::EventType::MODULE_ERROR));

// No context check - fires during ANY view, including PIN entry
```

## Recommended Fix
Implement a context-aware notification system with suppression capabilities:

**1. Add quiet mode to ViewStack:**
```cpp
// In ViewStack.h
class ViewStack {
public:
    /**
     * Enter quiet mode - suppress non-critical toasts
     * @param criticalOnly If true, only critical alerts are shown
     */
    void enterQuietMode(bool criticalOnly = true);
    
    /**
     * Exit quiet mode - show buffered notifications
     */
    void exitQuietMode();
    
    /**
     * Check if in quiet mode
     */
    bool isQuietMode() const { return quietMode_; }
    
private:
    bool quietMode_ = false;
    bool criticalOnly_ = false;
    std::vector<QueuedToast> toastBuffer_;  // Buffer for later display
};
```

**2. Implement quiet mode logic:**
```cpp
// In ViewStack.cpp
void ViewStack::enterQuietMode(bool criticalOnly) {
    quietMode_ = true;
    criticalOnly_ = criticalOnly;
}

void ViewStack::exitQuietMode() {
    quietMode_ = false;
    // Show buffered toasts
    if (!toastBuffer_.empty()) {
        // Show first buffered toast
        const auto& toast = toastBuffer_.front();
        showToastInternal(toast.message, toast.icon, toast.durationMs, toast.dismissible);
        toastBuffer_.erase(toastBuffer_.begin());
    }
}

// Helper to check if toast should show
static bool shouldShowToast(bool isCritical) {
    auto& stack = ViewStack::instance();
    if (!stack.isQuietMode()) return true;
    return isCritical || !stack.criticalOnly_;
}
```

**3. Update ToastView to check context:**
```cpp
// In ToastView.cpp
static void showToastInternal(const char* message, ToastView::Icon icon, uint16_t durationMs,
                              bool dismissible = true) {
    bool isCritical = (icon == ToastView::Icon::ALERT || icon == ToastView::Icon::ERROR);
    
    if (!shouldShowToast(isCritical)) {
        // Buffer the toast
        auto& stack = ViewStack::instance();
        stack.toastBuffer_.push_back({message, icon, durationMs, dismissible});
        return;
    }
    
    s_sharedToast.init(message, icon, durationMs, dismissible);
    ViewStack::instance().showModal(&s_sharedToast);
    ViewStack::instance().render();
}
```

**4. Enter quiet mode during input views:**
```cpp
// In PinEntryView::onEnter
void PinEntryView::onEnter(void* context) {
    ViewStack::instance().enterQuietMode(true);  // Suppress non-critical toasts
    // ...
}

// In PinEntryView::onExit
void PinEntryView::onExit() {
    ViewStack::instance().exitQuietMode();  // Show buffered toasts
}
```

**5. Update module error handler:**
```cpp
// In ExpertMenuUi.cpp:285-301
void onModuleErrorEvent(const core::Event& evt) {
    // ...
    
    // Check if in quiet context
    if (ViewStack::instance().isQuietMode() && ViewStack::instance().criticalOnly_()) {
        // Log but don't show toast
        LOG_W(TAG, "Module error suppressed: %s", errMsg);
        return;
    }
    
    showToastError(errMsg, TOAST_DURATION_LONG_MS);
}
```

## References
- Focus-aware notifications: https://developer.apple.com/design/human-interface-guidelines/notifications
- Do Not Disturb patterns: https://material.io/design/platform-guidance/android-notifications.html

