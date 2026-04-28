---
title: "[LOW] Adafruit-GFX library uses sprintf for string formatting"
severity: LOW
domain: display
lens: data-exposure
labels:
  - "audit:security/data-exposure"
---

## Summary
The Adafruit-GFX library (vendor component) uses `sprintf` for string formatting in multiple locations. While this is a third-party library, the sprintf calls could potentially be used to expose data through the display.

**Locations:** `components/Adafruit-GFX/WString.cpp:81,99,361,367,379`

## Impact
- **Buffer overflow risk**: sprintf doesn't check destination buffer size
- **Display data exposure**: Formatted strings may include sensitive data
- **Vendor code**: Third-party library, harder to fix directly

## Evidence
File: `components/Adafruit-GFX/WString.cpp:81`
```cpp
sprintf(buf, "%d", value);
```

File: `components/Adafruit-GFX/WString.cpp:99`
```cpp
sprintf(buf, "%ld", value);
```

File: `components/Adafruit-GFX/WString.cpp:361,367,379`
```cpp
sprintf(buf, "%d", num);
sprintf(buf, "%ld", num);
```

These are used for converting numbers to strings for display rendering.

## Recommended Fix
1. Consider patching Adafruit-GFX to use snprintf instead
2. Or wrap the library with bounds-checked formatting
3. Document that numbers converted via String class use sprintf

Example fix:
```cpp
// In WString.cpp, replace sprintf with snprintf
sprintf(buf, "%d", value);  // Original
snprintf(buf, sizeof(buf), "%d", value);  // Fixed
```

## References
- Adafruit-GFX library: https://github.com/adafruit/Adafruit-GFX-Library
- sprintf vs snprintf: https://www.cplusplus.com/reference/cstdio/sprintf/
- Related to issue #21 (CalEPD printf usage)
