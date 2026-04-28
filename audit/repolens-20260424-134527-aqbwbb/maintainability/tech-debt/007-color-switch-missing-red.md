---
title: "[MEDIUM] Color switch statements missing EPD_RED handling in E-Paper drivers"
severity: MEDIUM
domain: maintainability
lens: tech-debt/robustness
labels:
  - robustness
  - display
  - color-handling
---

## Summary
Multiple E-Paper display driver files in the CalEPD component have switch statements on color values that handle only EPD_BLACK and EPD_WHITE but silently ignore EPD_RED (and potentially EPD_GREEN, EPD_BLUE, EPD_YELLOW for 7-color displays). This affects 6+ display driver files.

## Impact
- **Silent data loss**: Red color data is dropped without warning when drawing on tri-color displays
- **Visual bugs**: Users may see missing red elements without understanding why
- **Maintenance confusion**: Developers may add more colors unaware of the incomplete handling
- **Inconsistent behavior**: Some drivers handle all 3 colors (gdeh0154z90), others only 2

## Evidence

### Affected files with incomplete color handling:

**1. dke075z83.cpp** - Lines 238-244
```cpp
switch (color)
{
case EPD_BLACK:
    color = EPD_WHITE;
    break;
case EPD_WHITE:
    color = EPD_BLACK;
    break;
}
// EPD_RED not handled! Falls through silently
_black_buffer[i] = (_black_buffer[i] & (0xFF ^ (1 << (7 - x % 8))));
_red_buffer[i] = (_red_buffer[i] & (0xFF ^ (1 << (7 - x % 8))));
if (color == EPD_WHITE) return;
else if (color == EPD_BLACK) _black_buffer[i] = (_black_buffer[i] | (1 << (7 - x % 8)));
else if (color == EPD_RED) _red_buffer[i] = (_red_buffer[i] | (1 << (7 - x % 8)));
else  // <-- EPD_GREEN, EPD_BLUE, etc. fall through here!
```

**2. gdeh042Z98.cpp** - Lines 215-221
```cpp
switch (color) {
    case EPD_BLACK:
        color = EPD_WHITE;
        break;
    case EPD_WHITE:
        color = EPD_BLACK;
        break;
}
// EPD_RED not handled in switch!
```

**3. gdeh042Z96.cpp** - Lines 236-242
```cpp
switch (color)
{
case EPD_BLACK:
    color = EPD_WHITE;
    break;
case EPD_WHITE:
    color = EPD_BLACK;
    break;
}
// EPD_RED not handled in switch!
```

**4. gdew0583z83.cpp** - Lines 237-243
```cpp
switch (color)
{
case EPD_BLACK:
    color = EPD_WHITE;
    break;
case EPD_WHITE:
    color = EPD_BLACK;
    break;
}
// EPD_RED not handled in switch!
```

**5. dke/dke075z83.cpp** - Same pattern as above

### Correct implementation example:
**gdeh0154z90.cpp** - Lines 83-96 (handles all 3 colors properly):
```cpp
switch (color)
{
case EPD_BLACK:
    _red_buffer[i] = (_red_buffer[i] & (GDEH0154Z90_8PIX_RED_WHITE ^ (1 << (7 - x % 8))));
    _black_buffer[i] = (_black_buffer[i] & (GDEH0154Z90_8PIX_WHITE ^ (1 << (7 - x % 8))));
    break;
case EPD_WHITE:
    _black_buffer[i] = (_black_buffer[i] | (1 << (7 - x % 8)));
    _red_buffer[i] = (_red_buffer[i] & (GDEH0154Z90_8PIX_RED_WHITE ^ (1 << (7 - x % 8))));
    break;
case EPD_RED:
    _red_buffer[i] = (_red_buffer[i] | (1 << (7 - x % 8))));
    _black_buffer[i] = (_black_buffer[i] & (GDEH0154Z90_8PIX_WHITE ^ (1 << (7 - x % 8))));
    break;
}
```

## Recommended Fix

### For each affected file, add EPD_RED case to switch:

**Before (incomplete):**
```cpp
switch (color)
{
case EPD_BLACK:
    color = EPD_WHITE;
    break;
case EPD_WHITE:
    color = EPD_BLACK;
    break;
}
```

**After (complete):**
```cpp
switch (color)
{
case EPD_BLACK:
    color = EPD_WHITE;
    break;
case EPD_WHITE:
    color = EPD_BLACK;
    break;
case EPD_RED:
    // Keep red as-is for tri-color displays
    break;
default:
    // Handle 7-color displays or log warning
    break;
}
```

### Files to fix (in order):
1. `components/CalEPD/models/color/dke075z83.cpp` - Line 238
2. `components/CalEPD/models/color/gdeh042Z98.cpp` - Line 215
3. `components/CalEPD/models/color/gdeh042Z96.cpp` - Line 236
4. `components/CalEPD/models/color/gdew0583z83.cpp` - Line 237
5. `components/CalEPD/models/color/dke/dke075z83.cpp` - Line 237

### Additional consideration for 7-color displays:
For displays supporting more colors (wave5i7Color, gdeq042Z21), add cases for:
- `EPD_GREEN`
- `EPD_BLUE`
- `EPD_YELLOW`

## References
- E-Paper display controller datasheets in `components/CalEPD/`
- Color definitions: `components/CalEPD/include/gdew_colors.h`, `components/CalEPD/include/color/wave7colors.h`
- Working example: `components/CalEPD/models/color/gdeh0154z90.cpp`
