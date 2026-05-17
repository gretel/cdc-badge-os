---
title: "[LOW] Dead Code: Multiple commented debug printf statements in CalEPD display drivers"
severity: LOW
domain: dead-code
lens: code-quality
labels:
  - "audit:code-quality/dead-code"
---

## Summary
Numerous CalEPD display driver files contain commented-out debug printf statements that were left in place after development/testing. These are scattered across approximately 20+ locations in the codebase.

**Sample locations:**
- `components/CalEPD/models/wave12i48.cpp:41` - Heap size check
- `components/CalEPD/models/gdem029E97.cpp:153` - Loop debug
- `components/CalEPD/models/gdew075T7Grays.cpp:224-394` - Multiple buffer/LUT debug prints
- `components/CalEPD/models/color/gdeq042Z21.cpp:142-200` - Color buffer debug
- `components/CalEPD/models/color/gdeh042Z21.cpp:142-200` - Color buffer debug
- `components/CalEPD/models/plasticlogic/plasticlogic014.cpp:41` - Temperature debug
- `components/CalEPD/models/plasticlogic/plasticlogic011.cpp:42` - Temperature debug

**Example (wave12i48.cpp:41):**
```cpp
    //printf("\nAvailable heap after Epd init:%d\n", (int) xPortGetFreeHeapSize());
```

**Example (gdem029E97.cpp:153):**
```cpp
  //printf("Loop from ys:%d to ye:%d\n", y, ye);
```

**Example (gdew075T7Grays.cpp:224):**
```cpp
        //printf("%x ",temp1);
        //printf("W ");
        //printf("G2 ");
        //printf("%x ", temp3);
        //printf("%x ",lut_ww.data[i]);
```

## Impact
- **Visual clutter**: ~20+ commented debug lines across multiple files
- **Minor maintenance**: Each line could be removed independently
- **Low priority**: These are simple debug statements that don't affect functionality

## Evidence
The commented statements include:
1. **Heap/memory debug** - `xPortGetFreeHeapSize()` calls
2. **Loop iteration debug** - Loop counter and range printing
3. **Buffer content debug** - Hex dumps of buffer data
4. **LUT debug** - Look-up table value printing
5. **Temperature debug** - EPD temperature readings

These appear to be development debugging that was left in place rather than removed.

## Recommended Fix
For each file with commented debug statements:

1. **Batch removal**: Remove all commented printf lines in a single edit per file
2. **Keep if useful**: If a particular debug statement is used regularly, consider:
   - Making it conditional on a DEBUG macro
   - Using the project's logging system (cdc_log) instead

Example fix:
```cpp
    // Before:
    //printf("\nAvailable heap after Epd init:%d\n", (int) xPortGetFreeHeapSize());
    
    // After:
    // (line removed)
```

For files with many debug statements (like gdew075T7Grays.cpp), consider a bulk removal:
```cpp
    // Before:
    //printf("%x ",temp1);
    //printf("W ");
    //printf("G2 ");
    //printf("%x ", temp3);
    
    // After:
    // (all 4 lines removed)
```

## References
- Debug code cleanup best practices
- Conditional compilation for debug output
