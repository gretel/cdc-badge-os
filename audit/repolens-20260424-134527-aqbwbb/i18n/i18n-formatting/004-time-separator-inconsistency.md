---
title: "[LOW] Time separator inconsistency (spaces around colon)"
severity: LOW
domain: i18n
lens: i18n-formatting
labels:
  - audit:i18n/i18n-formatting
---

## Summary

The `TimeInputView` component uses a **different time separator format** (` : ` with spaces) compared to all other time display locations in the application which use a simple colon (`:`). This creates visual inconsistency across the UI.

### Files Affected

| File | Lines | Time Format |
|------|-------|-------------|
| `components/cdc_views/src/TimeInputView.cpp` | 191 | `%02d : %02d` (with spaces) |
| `components/cdc_os_ui/src/SettingsHandlers.cpp` | 131 | `%02d:%02d` (no spaces) |
| `components/cdc_os_ui/src/SleepManager.cpp` | 162 | `%02d:%02d` (no spaces) |
| `components/cdc_os_ui/src/AppUi.cpp` | 261, 563, 713 | `%02d:%02d` (no spaces) |
| `components/cdc_hal/src/Rtc.cpp` | 123 | `%H:%M` (no spaces) |
| `components/serial_cmd/src/SerialCmd.cpp` | 669, 716 | `%02d:%02d:%02d` (no spaces) |

### Evidence

**TimeInputView.cpp:191** - Uses spaces around colon:
```cpp
snprintf(timeStr, sizeof(timeStr), "%02d : %02d", hour_, minute_);
```

**All other locations** - Use simple colon:
```cpp
// SettingsHandlers.cpp:131
snprintf(buf, sizeof(buf), "%02d:%02d", tm->tm_hour, tm->tm_min);

// Rtc.cpp:123
strftime(buf, bufLen, "%H:%M", &timeinfo);
```

## Impact

1. **Visual Inconsistency**: When users enter time in `TimeInputView` and then see it displayed elsewhere (e.g., in settings or lock screen), the format changes, which may cause momentary confusion.

2. **Minor UX Issue**: The extra spaces in `TimeInputView` make the time display slightly wider, potentially causing layout shifts when transitioning between views.

3. **Low Priority**: This is primarily a cosmetic issue rather than a functional problem. The time values are correct regardless of separator style.

## Recommended Fix

### Option 1: Match Standard Format (Recommended)

Update `TimeInputView.cpp` to use the same format as all other components:

```cpp
// Change from:
snprintf(timeStr, sizeof(timeStr), "%02d : %02d", hour_, minute_);

// To:
snprintf(timeStr, sizeof(timeStr), "%02d:%02d", hour_, minute_);
```

Also update the cursor position calculation since the string will be 2 characters shorter:

```cpp
// Line 206: Adjust charWidth calculation
// Current: int charWidth = 18;  // Based on "HH : MM" (7 chars)
// New:     int charWidth = 16;  // Based on "HH:MM" (5 chars)
```

### Option 2: Make Separator Configurable

If the spaced format is preferred for visual reasons, consider:
- Adding a format preference setting
- Documenting the intentional difference
- Ensuring the layout calculations account for the wider format consistently

## References

- The existing `Rtc.cpp` interface uses `strftime` with `%H:%M` format (no spaces)
- Most digital clocks and time displays use `HH:MM` format without spaces
- ISO 8601 time format uses `HH:MM:SS` (no spaces, or use `T` separator for full datetime)

</content>