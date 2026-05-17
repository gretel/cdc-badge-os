---
title: "[LOW] Dead Code: Commented-out member variable declarations in CalEPD display headers"
severity: LOW
domain: dead-code
lens: code-quality
labels:
  - "audit:code-quality/dead-code"
---

## Summary
Multiple CalEPD display driver header files contain commented-out member variable declarations that appear to be legacy code from different buffer allocation strategies. These commented declarations add visual noise but serve no functional purpose.

**Locations identified:**
- `components/CalEPD/include/gdew042t2Grays.h:56` - `//uint16_t _partials = 0;`
- `components/CalEPD/include/gdew075T7.h:60` - `//uint8_t* _buffer = (uint8_t*)heap_caps_malloc(...)`
- `components/CalEPD/include/goodisplay/gdey075T7.h:61` - `//uint8_t* _buffer = ...`
- `components/CalEPD/include/goodisplay/gdey0583T81.h:60` - `//uint8_t* _buffer = ...`
- `components/CalEPD/include/wave12i48.h:54` - `//uint8_t _buffer[WAVE12I48_BUFFER_SIZE];`
- `components/CalEPD/include/color/gdey073d46.h:40` - `//uint8_t _buffer[GDEY073D46_BUFFER_SIZE];`

**Example (gdew042t2Grays.h:56):**
```cpp
    bool _initial = true;
    bool _partial_mode = false;
    //uint16_t _partials = 0;
    
    uint16_t _setPartialRamArea(uint16_t x, uint16_t y, uint16_t xe, uint16_t ye);
```

**Example (gdew075T7.h:60):**
```cpp
    // Place _buffer in external RAM
    //uint8_t* _buffer = (uint8_t*)heap_caps_malloc(GDEW075T7_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
```

## Impact
- **Visual clutter**: Commented declarations make headers harder to read
- **Confusion**: Developers may wonder if these should be enabled for specific configurations
- **Maintenance**: If the active code changes, these comments may become outdated

## Evidence
The commented variables appear to be:
1. **Alternative buffer declarations** - Different files show different buffer allocation strategies (static array vs dynamic heap allocation)
2. **Deprecated counters** - `_partials` counter that is no longer used

These are not explanatory comments but actual code declarations that were commented out rather than removed.

## Recommended Fix
For each header file:
1. **Verify the variable is truly unused** - Search the entire class for any usage of the variable name
2. **Remove the commented line** if unused:
```cpp
    bool _initial = true;
    bool _partial_mode = false;
    
    uint16_t _setPartialRamArea(uint16_t x, uint16_t y, uint16_t xe, uint16_t ye);
```

3. **If the variable might be needed for specific configurations**, consider:
   - Adding a feature flag
   - Moving to a conditional compilation block with `#ifdef`
   - Adding a more descriptive comment explaining why it's commented

## References
- C++ member variable best practices
- Code cleanup: Remove dead code instead of commenting it out
