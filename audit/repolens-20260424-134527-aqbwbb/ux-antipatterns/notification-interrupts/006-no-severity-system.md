---
title: "[MEDIUM] Toast system lacks severity-based visual treatment"
severity: MEDIUM
domain: notification-interrupts
lens: notification-interrupts
labels:
  - "toast-implementation"
  - "severity-system"
---

## Summary
The ToastView system has icon types (SUCCESS, ERROR, INFO, ALERT) but all toasts share the same visual treatment, duration, and positioning regardless of severity. There is no priority-based escalation for critical errors.

**Files affected:**
- `components/cdc_views/include/cdc_views/ToastView.h:20-29`
- `components/cdc_views/src/ToastView.cpp:15-248`

**Evidence:**
```cpp
// ToastView.h:20-29 - Icon types exist but no severity differentiation
enum class Icon : uint8_t {
    NONE = 0,
    SUCCESS,
    ERROR,
    INFO,
    TASK,
    ALERT
};

// All toast functions use same default duration
void showToastSuccess(const char* message, uint16_t durationMs = 1500);
void showToastError(const char* message, uint16_t durationMs = 1500);
void showToastInfo(const char* message, uint16_t durationMs = 1500);

// No automatic duration adjustment based on severity
// Error toasts get same 1500ms as success toasts
```

**Current behavior:**
```cpp
// Both get same treatment despite different importance
ui::showToastSuccess("OK");        // 1500ms, checkmark icon
ui::showToastError("Failed");      // 1500ms, X icon
ui::showToastAlertSticky("Slot map invalid");  // Never dismisses, alert icon
```

## Impact
- **Critical errors missed**: Error toasts auto-dismiss as quickly as info toasts
- **No escalation path**: Critical errors should be more persistent
- **Accessibility gap**: No ARIA-like distinction for screen readers (though less relevant for embedded)
- **Visual hierarchy missing**: All toasts look equally important

## Evidence
1. Icon enum exists but only affects icon rendering, not behavior
2. All toast functions have same default duration (1500ms)
3. No automatic duration adjustment: ERROR toasts don't get more time than INFO
4. `showToastAlertSticky()` is the only escalation, but it's all-or-nothing (never dismisses)

## Recommended Fix
Implement severity-based duration and behavior:

```cpp
// Add to ToastView.h
enum class Severity {
    INFO = 0,      // 1500ms, auto-dismiss
    SUCCESS,       // 1500ms, auto-dismiss
    WARNING,       // 2500ms, auto-dismiss
    ERROR,         // 3000ms, auto-dismiss
    CRITICAL       // 5000ms or manual dismiss
};

// Update init signature
void init(const char* message, Icon icon, Severity severity, bool dismissible = true);

// Helper to get duration based on severity
static uint16_t getDurationForSeverity(Severity severity) {
    switch (severity) {
        case Severity::INFO: return 1500;
        case Severity::SUCCESS: return 1500;
        case Severity::WARNING: return 2500;
        case Severity::ERROR: return 3000;
        case Severity::CRITICAL: return 5000;
        default: return 1500;
    }
}

// Update convenience functions
void showToastError(const char* message, uint16_t durationMs = 0) {
    uint16_t dur = durationMs > 0 ? durationMs : getDurationForSeverity(Severity::ERROR);
    showToastInternal(message, Icon::ERROR, dur);
}
```

**Alternative (simpler):**
```cpp
// Just adjust defaults per function
void showToastError(const char* message, uint16_t durationMs = 2500);  // Was 1500
void showToastSuccess(const char* message, uint16_t durationMs = 1500);  // Keep
void showToastInfo(const char* message, uint16_t durationMs = 1500);  // Keep
```

## References
- WCAG 2.1: Focus order and priority
- Material Design: Snackbars and severity levels
