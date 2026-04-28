---
title: "[LOW] Spelling errors in comments (umlauten, bonds)"
severity: LOW
domain: code-quality/comments
lens: comments
labels:
  - "audit:code-quality/comments"
---

## Summary
Spelling errors found in comments across the CalEPD component:

1. **"umlauten"** should be **"umlauts"** (in 3 files)
2. **"bonds"** should be **"bounds"** (in 1 file)

## Impact
- **Professionalism**: Typos in comments reduce code quality perception
- **Clarity**: "umlauten" is German plural but English comment should use "umlauts"
- **Searchability**: Developers searching for "bounds" won't find "bonds"

## Evidence

**Files with "umlauten" typo** (should be "umlauts"):
- `components/CalEPD/epd.cpp:21`
- `components/CalEPD/epdParallel.cpp:21`
- `components/CalEPD/epd7color.cpp:21`

```cpp
uint8_t _unicodePerChar(uint8_t c) {
  // Cope with umlauten - Needs work  // Should be "umlauts"
     // TODO: Research a smarter way to do this
```

**File with "bonds" typo**:
- `components/CalEPD/models/color/wave5i7ColorVector.cpp:235`

```cpp
// #43 TODO: Check why is trying to update out of bonds anyways  // Should be "bounds"
```

## Recommended Fix
Correct the spelling in each file:

1. **epd.cpp:21** - Change `// Cope with umlauten - Needs work` to `// Handle umlauts and special characters`
2. **epdParallel.cpp:21** - Same change
3. **epd7color.cpp:21** - Same change
4. **wave5i7ColorVector.cpp:235** - Change `// out of bonds` to `// out of bounds`

Bonus: Consider improving the comment quality:
```cpp
// Map extended ASCII to Adafruit GFX character codes
// Note: German umlauts (ä, ö, ü, Ä, Ö, Ü), ß, and European characters
```

## References
- "Umlaut" is German for "sound change"; English plural is "umlauts"
- "Bounds" means limits/borders; "bonds" means connections/ties
