---
title: "[LOW] E-Paper display uses vsnprintf for text formatting, potentially exposing data on screen"
severity: LOW
domain: cdc_hal
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The E-Paper display component uses `vsnprintf` to format text strings for display. This means any data passed to the display's printf-like functions is rendered on the screen, potentially exposing sensitive information visually.

**Location:** `components/cdc_hal/src/EpaperDisplay.cpp:502`

## Impact
- **Visual data exposure**: Formatted strings are rendered on the E-Paper display
- **Persistent display**: E-Paper retains image until refreshed, data may remain visible
- **No filtering**: All formatted data is displayed without redaction
- **Screen reading**: Physical access allows reading of displayed data

## Evidence
File: `components/cdc_hal/src/EpaperDisplay.cpp:502`
```cpp
vsnprintf(buf, sizeof(buf), fmt, args);
```

This is used in the `printf` method of the display class to format text for rendering. The display is used throughout the UI for:
- Menu items
- Status information
- TOTP codes
- PIN entry prompts
- Notification messages

## Recommended Fix
1. Add a debug-only conditional for detailed formatting
2. Consider adding a redaction layer for sensitive patterns
3. Document that display data is visible to anyone with physical access

Example fix:
```cpp
void EpaperDisplay::printf(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    // Optional: Redact sensitive patterns
    #ifdef DEBUG_MODE
    // Only show detailed format in debug
    displayText(buf);
    #endif
}
```

## References
- E-Paper display characteristics: https://www.waveshare.com/wiki/7.5inch_e-Paper_HAT
- Visual data exposure: https://en.wikipedia.org/wiki/Shoulder_surfing
- Related to issue #21 (CalEPD printf usage)
