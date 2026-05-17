---
title: "[MEDIUM] Dead Code: Multiple commented-out _initial if blocks in CalEPD display drivers"
severity: MEDIUM
domain: dead-code
lens: code-quality
labels:
  - "audit:code-quality/dead-code"
---

## Summary
Multiple CalEPD display driver files contain identical commented-out `if (_initial)` blocks that were marked as "Original deprecated" but left in place. These blocks appear in 5 different files, suggesting a systematic refactoring that was never completed.

**Locations:**
- `components/CalEPD/models/color/gdeh042Z98.cpp:143`
- `components/CalEPD/models/color/gdeq042Z21.cpp:159`
- `components/CalEPD/models/color/gdeh042Z96.cpp:160`
- `components/CalEPD/models/color/gdeh042Z21.cpp:159`
- `components/CalEPD/models/gdew042t2.cpp:295`

**Example (gdeh042Z98.cpp):**
```cpp
// GxEPD comment: Avoid double full refresh after deep sleep wakeup
// if (_initial) {  // --> Original deprecated

printf("\n\nSTATS (ms)\n%llu _wakeUp settings+send Buffer\n%llu _powerOn\n%llu total time in millis\n",
       (endTime - startTime) / 1000, (powerOnTime - endTime) / 2000, (powerOnTime - startTime) / 1000);

_initial = false;  // This line still exists!
```

## Impact
- **Code clutter**: 5 files with similar dead code increases maintenance burden
- **Confusion**: Developers may wonder if the deprecated code should be re-enabled
- **Inconsistency**: The `_initial = false` assignment still exists, suggesting incomplete refactoring

## Evidence
All 5 files show the same pattern:
1. Commented `if (_initial) {` block with "Original deprecated" comment
2. Active `printf` stats code that was presumably inside the if block
3. Active `_initial = false;` assignment

Example from `gdeh042Z98.cpp:143`:
```cpp
    // GxEPD comment: Avoid double full refresh after deep sleep wakeup
    // if (_initial) {  // --> Original deprecated

    printf("\n\nSTATS (ms)\n%llu _wakeUp settings+send Buffer\n%llu _powerOn\n%llu total time in millis\n",
           (endTime - startTime) / 1000, (powerOnTime - endTime) / 1000, (powerOnTime - startTime) / 1000);

    _sleep();
    _initial = false;  // Line 155 - still active!
```

## Recommended Fix
For each of the 5 files:

1. **Remove the commented line** if the stats printf should always run:
```cpp
    // GxEPD comment: Avoid double full refresh after deep sleep wakeup
    // if (_initial) {  // --> Original deprecated

    printf("\n\nSTATS (ms)\n...");  // Keep this
    _sleep();
    _initial = false;  // Keep this
```

2. **Or remove the `_initial` member variable** if it's no longer used anywhere:
   - Check if `_initial` is used elsewhere in the class
   - If not, remove the member declaration from the header file

## References
- Dead code detection best practices
- Code refactoring: Remove commented-out code and use version control history
