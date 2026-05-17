---
title: "[LOW] Dead Code: Commented printf debug statement in Adafruit GFX library"
severity: LOW
domain: dead-code
lens: code-quality
labels:
  - "audit:code-quality/dead-code"
---

## Summary
In the Adafruit GFX library, a commented-out printf debug statement exists at `components/Adafruit-GFX/Adafruit_GFX.cpp:1264`. This appears to be a debug statement left in place from development/testing.

**Location:** `components/Adafruit-GFX/Adafruit_GFX.cpp:1264`

```cpp
        //printf("write() >=%d <=%d CHAR is in range\n",first, (uint8_t)pgm_read_byte(&gfxFont->last));
```

## Impact
- **Minor visual clutter**: Single debug line in a third-party library
- **Low priority**: Since this is a third-party library (Adafruit-GFX), it may be updated independently

## Evidence
File: `components/Adafruit-GFX/Adafruit_GFX.cpp`
- Line 1264: Debug printf statement showing character range checking

This is in the `write()` method that handles character rendering. The debug statement would show the range of valid characters for a font.

## Recommended Fix
Since this is in a third-party library (Adafruit-GFX):

1. **Option 1 - Leave as-is**: The library may be updated from upstream, so minimal changes
2. **Option 2 - Remove**: If maintaining a clean local copy, remove the commented line

Example:
```cpp
        // Debug line removed
```

Note: This is lower priority since it's in a third-party dependency.

## References
- Third-party library maintenance
- Debug code cleanup
