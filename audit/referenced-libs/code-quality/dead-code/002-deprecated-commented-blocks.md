---
title: "[LOW] Dead commented code blocks: deprecated _initial checks in CalEPD"
severity: LOW
domain: dead-code
lens: dead-code
labels:
  - "audit:code-quality/dead-code"
---

## Summary
Multiple CalEPD display model files contain commented-out code blocks with explicit markers indicating they are deprecated. These blocks are no longer functional but remain in the source files.

**Locations:**
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/CalEPD/models/color/gdeh042Z98.cpp:143`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/CalEPD/models/color/gdeq042Z21.cpp:159`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/CalEPD/models/color/gdeh042Z96.cpp:160`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/CalEPD/models/color/gdeh042Z21.cpp:159`
- `/input/20260423-132359-oj8ayc/cdc-badge-os/components/CalEPD/models/gdew042t2.cpp:295`

All contain the same pattern:
```cpp
// if (_initial) {  // --> Original deprecated
```

## Impact
- **Visual clutter**: Commented code adds noise to already complex display driver code
- **Maintenance overhead**: Developers may wonder if the code should be restored
- **Repository size**: Small but cumulative impact across 5 files

## Evidence
From `components/CalEPD/models/color/gdeh042Z98.cpp:142-144`:
```cpp
    // GxEPD comment: Avoid double full refresh after deep sleep wakeup
    // if (_initial) {  // --> Original deprecated

    printf("\n\nSTATS (ms)\n%llu _wakeUp settings+send Buffer\n%llu _powerOn\n%llu total time in millis\n",
```

The comment explicitly states this is "Original deprecated" code, indicating it was intentionally disabled and should be removed.

## Recommended Fix
For each of the 5 affected files, remove the commented line:
```bash
# Example for gdeh042Z98.cpp
sed -i '143d' components/CalEPD/models/color/gdeh042Z98.cpp
```

Or manually edit each file to remove:
- `// if (_initial) {  // --> Original deprecated`

Keep the explanatory comment "GxEPD comment: Avoid double full refresh after deep sleep wakeup" as it provides context.

## References
- Clean Code by Robert C. Martin: "Use dead code as temporary storage. But only temporarily!"
- https://www.quora.com/Should-commented-out-code-be-removed-from-the-codebase
