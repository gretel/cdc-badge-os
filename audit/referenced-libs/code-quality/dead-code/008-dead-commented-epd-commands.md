---
title: "[LOW] Dead Code: Commented EPD controller commands in CalEPD display drivers"
severity: LOW
domain: dead-code
lens: code-quality
labels:
  - "audit:code-quality/dead-code"
---

## Summary
Multiple CalEPD display driver files contain commented-out EPD controller commands (IO.cmd and IO.data calls). These are approximately 28 instances across 15+ files, representing alternative configurations or experimental code that was disabled but not removed.

**Sample locations:**
- `components/CalEPD/models/gdem029E97.cpp:251-294` - Driver output control and LUT commands
- `components/CalEPD/models/color/gdeh042Z98.cpp:136-137` - Display update control
- `components/CalEPD/models/color/wave4i7Color.cpp:71-72` - PLL control
- `components/CalEPD/models/gdew042t2.cpp:226,316` - Wakeup and visual commands
- `components/CalEPD/models/custom/custom042.cpp:69-70,194,271,293,332` - Multiple experimental commands
- `components/CalEPD/models/dke/depg750bn.cpp:106` - Software reset

**Examples:**

Driver output control (gdem029E97.cpp:251):
```cpp
  //IO.cmd(0x01); //Driver output control      
  //IO.data(0x27);
```

Display update control (gdeh042Z98.cpp:136):
```cpp
//    IO.cmd(0x22);  //Display Update Control
//    IO.data(0xC7);
```

PLL control (wave4i7Color.cpp:71):
```cpp
  //IO.cmd(0x30); // PLL Control
  //IO.data(0x3C);    // 50 Hz
```

Experimental commands (custom042.cpp:69-70):
```cpp
  //IO.data(0x37); //0x37 -> Lo da vuelta
  //IO.data(0x8A);
```

## Impact
- **Visual clutter**: 28+ commented command lines across 15 files
- **Confusion**: Developers may wonder if these commands should be enabled for specific configurations
- **Experimental code**: Comments like "Not any visual difference" suggest these were tested and found unnecessary

## Evidence
The commented commands fall into categories:
1. **Driver output control** - `IO.cmd(0x01)` with data values
2. **Display update control** - `IO.cmd(0x22)` with various data
3. **PLL control** - `IO.cmd(0x30)` for frequency
4. **Wakeup power** - `epd_wakeup_power` data sequences
5. **Experimental commands** - Various with notes like "Lo da vuelta" (Spanish for "flips it")
6. **Visual difference tests** - Commands marked "Not any visual difference"

Example context from `custom042.cpp:271`:
```cpp
  //IO.data(0x01);         // Not any visual difference
  IO.cmd(0x11); // scan direction
  IO.data(0x03);
```

## Recommended Fix
For each file with commented commands:

1. **Review the comment** - Determine if the command was:
   - Tested and found unnecessary (e.g., "Not any visual difference") → Remove
   - Alternative configuration → Consider feature flag or remove
   - Experimental → Remove if no longer needed

2. **Batch removal** - Remove all commented commands in a single edit per file

3. **Document if keeping** - If a command might be useful for future reference, move to a `// REMEMBER:` comment section

Example fix:
```cpp
  // Before:
  //IO.data(0x01);         // Not any visual difference
  IO.cmd(0x11); // scan direction
  
  // After:
  IO.cmd(0x11); // scan direction
```

## References
- EPD controller command documentation
- Code cleanup: Remove experimental code that's been tested
