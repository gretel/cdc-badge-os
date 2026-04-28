---
title: "[MEDIUM] Toast system lacks queue management - unlimited simultaneous toasts"
severity: MEDIUM
domain: notification-interrupts
lens: notification-interrupts
labels:
  - "toast-implementation"
  - "queue-management"
---

## Summary
The ToastView system uses a single shared static instance (`s_sharedToast`) with no queue management. When multiple toasts are triggered in rapid succession, they overwrite each other without any buffering or queuing mechanism.

**Files affected:**
- `components/cdc_views/src/ToastView.cpp:173-180`
- `components/cdc_views/include/cdc_views/ToastView.h:37-38`

**Evidence:**
```cpp
// ToastView.cpp:173 - Single shared instance
static ToastView s_sharedToast;

// ToastView.cpp:175-180 - No queue, just overwrites
static void showToastInternal(const char* message, ToastView::Icon icon, uint16_t durationMs,
                              bool dismissible = true) {
    s_sharedToast.init(message, icon, durationMs, dismissible);
    ViewStack::instance().showModal(&s_sharedToast);
    ViewStack::instance().render();  // Immediate render for toast
}
```

**Usage patterns that cause issues:**
```cpp
// Multiple toasts in rapid succession (e.g., from event handlers)
showToastError("Error 1");
showToastSuccess("Success 2");
showToastInfo("Info 3");
// Only the last toast will be visible
```

## Impact
- **Lost notifications**: Rapidly triggered toasts overwrite each other, losing important messages
- **No buffer**: When a toast is already showing, new toasts immediately replace it
- **Race conditions**: Toasts triggered from different modules may conflict

## Evidence
1. Single static instance `s_sharedToast` with no array/queue
2. `showToastInternal()` directly calls `showModal()` without checking if a toast is already visible
3. `ViewStack::showModal()` replaces the current modal without preserving previous state
4. ~100 showToast calls across the codebase, many in event handlers that could fire rapidly

## Recommended Fix
Implement a simple toast queue with a maximum depth:

```cpp
// Add to ToastView.h
class ToastView {
public:
    // ... existing ...
    static void showToast(const char* message, Icon icon, uint16_t durationMs, bool dismissible);
};

// Add to ToastView.cpp
static constexpr uint8_t MAX_TOAST_QUEUE = 3;
struct ToastEntry {
    char message[64];
    ToastView::Icon icon;
    uint16_t durationMs;
    bool dismissible;
};
static ToastEntry toastQueue[MAX_TOAST_QUEUE];
static uint8_t queueHead = 0;
static uint8_t queueTail = 0;

static void showToastInternal(...) {
    // Check if toast is already showing
    if (ViewStack::instance().hasModal()) {
        // Add to queue if space available
        if ((queueHead + 1) % MAX_TOAST_QUEUE != queueTail) {
            toastQueue[queueHead] = {message, icon, durationMs, dismissible};
            queueHead = (queueHead + 1) % MAX_TOAST_QUEUE;
        }
        return;
    }
    
    // Show toast
    s_sharedToast.init(message, icon, durationMs, dismissible);
    ViewStack::instance().showModal(&s_sharedToast);
    
    // Check queue for next toast
    if (queueTail != queueHead) {
        queueTail = (queueTail + 1) % MAX_TOAST_QUEUE;
        // Dequeue next toast after current duration
        // (requires timer callback or tick integration)
    }
}
```

## References
- Toast queue patterns in mobile UI frameworks
- Notification deduplication strategies
