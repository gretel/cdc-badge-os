---
title: "[MEDIUM] TODO comments in CalEPD component need resolution or conversion to issues"
severity: MEDIUM
domain: code-quality/comments
lens: comments
labels:
  - "audit:code-quality/comments"
---

## Summary
Multiple TODO comments found in the CalEPD component (E-Paper display library) lack clear ownership, timelines, or tracking issue references. These TODOs have accumulated across several files:

1. **components/CalEPD/epd.cpp:8** - `// TODO: Implement printf`
2. **components/CalEPD/epd.cpp:22** - `// TODO: Research a smarter way to do this` (German umlaut handling)
3. **components/CalEPD/epdParallel.cpp:8** - `// TODO: Implement printf`
4. **components/CalEPD/epdParallel.cpp:22** - `// TODO: Research a smarter way to do this` (German umlaut handling)
5. **components/CalEPD/epd7color.cpp:8** - `// TODO: Implement printf`
6. **components/CalEPD/epd7color.cpp:22** - `// TODO: Research a smarter way to do this` (German umlaut handling)
7. **components/CalEPD/include/plasticlogic021.h:19** - `// TODO: Should be 2 bits per pixel:` (incomplete comment)
8. **components/CalEPD/include/plasticlogic031.h:19** - `// TODO: Should be 2 bits per pixel:` (incomplete comment)
9. **components/CalEPD/models/fix/gdeh0213b73.cpp:74** - `// TODO` (empty, in `initPartialUpdate()`)
10. **components/CalEPD/models/goodisplay/gdeq037T31.cpp:187** - `// TODO: Refactor this in 4 different LUT constants and sent in a whole CS toggle`
11. **components/CalEPD/models/plasticlogic/plasticlogic.cpp:272** - `.TODO: Implement printf`

## Impact
- **Maintainability**: TODO comments without tracking issues become forgotten technical debt
- **Code clarity**: Incomplete comments (like plasticlogic021.h line 19) confuse developers
- **Duplication**: Same TODO ("Implement printf", "Research a smarter way") appears in 3+ files, suggesting either copy-paste or related work that should be addressed together
- **Missed optimization**: The LUT refactor TODO in gdeq037T31.cpp could improve performance by reducing SPI chip-select toggles

## Evidence
```cpp
// components/CalEPD/epd.cpp:8
// TODO: Implement printf
size_t Epd::write(uint8_t v){
  Adafruit_GFX::write(v);
  return 1;
}

// components/CalEPD/include/plasticlogic021.h:19
// TODO: Should be 2 bits per pixel: 
#define PLOGIC021_BUFFER_SIZE (uint32_t(PLOGIC021_WIDTH) * uint32_t(PLOGIC021_HEIGHT) / 4)

// components/CalEPD/models/fix/gdeh0213b73.cpp:74
// TODO
    if (debug_enabled) printf("initPartialUpdate() Not impemented \n");
```

## Recommended Fix
1. **For "Implement printf" TODOs**: Either implement the feature or delete the TODO comment if it's low priority. Note: `printerf()` method already exists in epd.cpp:100, so clarify if `printf()` wrapper is needed.

2. **For "Research a smarter way" umlaut handling**: 
   - If current implementation works adequately, replace TODO with a note explaining the approach
   - If refactor needed, create a tracking issue with specific requirements

3. **For plasticlogic021.h/031.h**: Complete the comment explaining the 2-bit pixel format or fix the buffer calculation

4. **For gdeh0213b73.cpp empty TODO**: Either implement partial update or add detail like `// TODO: Implement partial update initialization`

5. **For gdeq037T31.cpp LUT refactor**: Create a tracking issue with performance metrics and implementation plan

## References
- C++ Best Practices: "TODO comments should have a date, author, and preferably a tracking issue reference"
- Clean Code (Robert Martin): "Treat TODOs as debt - pay it back or acknowledge it's intentional"
