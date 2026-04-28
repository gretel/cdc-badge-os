---
title: "[MEDIUM] Unresolved TODO comments in CalEPD display component"
severity: MEDIUM
domain: maintainability
lens: tech-debt/todos
labels:
  - todos
  - display
---

## Summary
Multiple unresolved TODO comments exist in the CalEPD (E-Paper display) component, indicating incomplete implementations and refactoring opportunities that have accumulated over time.

## Impact
- **Incomplete features**: Partial implementations may cause unexpected behavior
- **Code quality**: Known issues left unaddressed degrade code maintainability
- **Performance**: Suboptimal implementations (e.g., inefficient LUT sending)
- **Debugging**: TODOs indicate areas where developers may encounter issues

## Evidence

### Critical TODOs in CalEPD component:

**1. Incomplete implementation - Partial update:**
File: `components/CalEPD/models/fix/gdeh0213b73.cpp`, Line 74
```cpp
void Gdeh0213b73::initPartialUpdate(){
// TODO
    if (debug_enabled) printf("initPartialUpdate() Not impemented \n");
}
```

**2. Refactoring opportunity - LUT optimization:**
File: `components/CalEPD/models/goodisplay/gdeq037T31.cpp`, Lines 186-188
```cpp
/**
 * @brief This is horrible like this since it's raising CS pin for EVERY byte sent
 *        TODO: Refactor this in 4 different LUT constants and sent in a whole CS toggle
 */
```

**3. Buffer calculation correction:**
File: `components/CalEPD/include/plasticlogic021.h`, Line 19
File: `components/CalEPD/include/plasticlogic031.h`, Line 19
```cpp
// TODO: Should be 2 bits per pixel: 
#define PLOGIC021_BUFFER_SIZE (uint32_t(PLOGIC021_WIDTH) * uint32_t(PLOGIC021_HEIGHT) / 4)
```

**4. Printf implementation missing:**
File: `components/CalEPD/epd.cpp`, Line 8
File: `components/CalEPD/epdParallel.cpp`, Line 8
File: `components/CalEPD/epd7color.cpp`, Line 8
```cpp
// TODO: Implement printf
```

**5. Unicode handling needs work:**
File: `components/CalEPD/epd.cpp`, Lines 21-22
```cpp
// Cope with umlauten - Needs work
// TODO: Research a smarter way to do this
```

## Recommended Fix

### Priority 1 - Buffer calculation (quick win):
Verify the correct pixel format for UC8156 controller and update buffer size calculation:
```cpp
// For 2 bits per pixel:
#define PLOGIC021_BUFFER_SIZE (uint32_t(PLOGIC021_WIDTH) * uint32_t(PLOGIC021_HEIGHT) * 2 / 8)
```

### Priority 2 - LUT refactoring:
Create separate LUT arrays and batch send with single CS toggle:
```cpp
static const uint8_t LUT_VCOM[] = { /* 42 bytes */ };
static const uint8_t LUT_WW[] = { /* 42 bytes */ };
static const uint8_t LUT_R[] = { /* 42 bytes */ };
static const uint8_t LUT_W[] = { /* 42 bytes */ };

void Gdeq037T31::_writeFullLut() {
    IO.cmd(0x20); // VCOM
    for (uint8_t i = 0; i < 42; i++) IO.data(LUT_VCOM[i]);
    IO.cmd(0x21); // WW
    for (uint8_t i = 0; i < 42; i++) IO.data(LUT_WW[i]);
    // ... etc
}
```

### Priority 3 - Document partial update:
If partial update is not needed, mark as intentionally omitted:
```cpp
void Gdeh0213b73::initPartialUpdate(){
    // Intentionally not implemented for GDEH0213B73
    // Full update only supported
}
```

## References
- E-Paper controller datasheets in `components/CalEPD/`
- Adafruit GFX library for printf implementation reference
