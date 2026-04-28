---
title: "[LOW] Toast durations too short for readability (1000ms default)"
severity: LOW
domain: notification-interrupts
lens: notification-interrupts
labels:
  - "toast-implementation"
  - "readability"
---

## Summary
Toast messages use a default duration of 1500ms (1.5 seconds), which may be too short for users to read longer messages, especially on E-Paper displays with slower refresh rates.

**Files affected:**
- `components/cdc_views/include/cdc_views/ToastView.h:37,73-98`
- `components/cdc_views/src/ToastView.cpp:188-248`
- `components/cdc_os_ui/src/AppUiInternal.h:28-30`

**Evidence:**
```cpp
// ToastView.h:37 - Default duration 1500ms
void init(const char* message, Icon icon = Icon::NONE, uint16_t durationMs = 1500, ...);

// ToastView.h:73 - Default duration 1500ms
void showToast(const char* message, uint16_t durationMs = 1500);

// AppUiInternal.h:28-30 - Short duration is only 1000ms
static constexpr uint32_t TOAST_DURATION_SHORT_MS = 1000;
static constexpr uint32_t TOAST_DURATION_MEDIUM_MS = 1500;
static constexpr uint32_t TOAST_DURATION_LONG_MS = 2500;

// Example usage with short duration
ui::showToastSuccess("Typed");  // No duration specified, uses default 1500ms
showToastInfo(tr(StringId::BLUETOOTH_OFF));  // No duration specified, uses default 1500ms
```

**Reading speed calculation:**
- Average reading speed: ~200-250 words per minute = ~3-4 words per second
- E-Paper display refresh: ~100-500ms (slower than LCD)
- 1500ms allows reading ~3-4 words maximum
- Messages like "Bluetooth: [SSID] connected" exceed this limit

## Impact
- **Missed information**: Users may not read the full message before it disappears
- **Accessibility issues**: Slower readers or users with cognitive disabilities affected
- **E-Paper specific**: Slower refresh rate makes short toasts harder to read

## Evidence
1. Default duration is 1500ms for most toast functions
2. Short duration constant is only 1000ms
3. Many toast calls use default duration without specifying a longer time
4. E-Paper display (GDEY029T94) has slower refresh than LCD

## Recommended Fix
Increase default toast duration to 2000-2500ms:

```cpp
// ToastView.h:37 - Increase default to 2000ms
void init(const char* message, Icon icon = Icon::NONE, uint16_t durationMs = 2000, ...);

// ToastView.h:73 - Increase default to 2000ms
void showToast(const char* message, uint16_t durationMs = 2000);

// AppUiInternal.h:28-30 - Adjust constants
static constexpr uint32_t TOAST_DURATION_SHORT_MS = 1500;   // Was 1000
static constexpr uint32_t TOAST_DURATION_MEDIUM_MS = 2000;  // Was 1500
static constexpr uint32_t TOAST_DURATION_LONG_MS = 3000;    // Was 2500
```

Or implement variable duration based on message length:
```cpp
void showToast(const char* message, uint16_t durationMs = 0) {
    if (durationMs == 0 && message) {
        // Calculate duration based on message length (50ms per character)
        uint16_t len = strlen(message);
        durationMs = max(2000, len * 50);  // Minimum 2000ms
    }
    showToastInternal(message, ToastView::Icon::NONE, durationMs);
}
```

## References
- Nielsen Norman Group: Time to Read
- WCAG 2.1: Timing Adjustable
