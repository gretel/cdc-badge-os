---
title: "[LOW] Confirmation dialog uses asymmetric button labels"
severity: LOW
domain: ui
lens: dark-patterns
labels:
  - "audit:ux-antipatterns/dark-patterns"
---

## Summary
The confirmation dialog uses asymmetric labels for confirm vs. decline actions. The confirm action uses "OK" (or "Ja" in German) which is action-oriented, while the decline action uses "Back" (or "Nein") which is neutral/positional.

**Evidence:**
- File: `components/cdc_views/src/ConfirmView.cpp`, line 185
- Code:
```cpp
gfx->print("Y=Ja  N=Nein");  // German footer hint
```

- File: `components/cdc_views/include/cdc_views/ConfirmView.h`, line 57
- Code:
```cpp
const char* getFooterHint() const override { return "Y=OK  N=Abbruch"; }
```

Note: The English hint in the header says "OK/Abbruch" while the actual rendered German text says "Ja/Nein" - there's a slight inconsistency.

## Impact
This is a very minor visual hierarchy asymmetry. "OK" is slightly more positive/action-oriented than "Back/Abbruch", which could subtly nudge users toward confirmation. However, the effect is minimal because:
1. The labels are in a small footer hint, not prominent buttons
2. "Back/Abbruch" is a neutral, commonly understood term
3. Both options are equally accessible via key press (Y/N)

## Recommended Fix
Use more neutral, symmetric labels:
- Change to `"Y=Yes  N=No"` or `"Y=Accept  N=Decline"` (English)
- Change to `"Y=Ja  N=Nein"` consistently (German - already correct in implementation)

Ensure both labels have the same tone and length for visual balance.

Example:
```cpp
// In components/cdc_views/include/cdc_views/ConfirmView.h, line 57
const char* getFooterHint() const override { return "Y=Yes  N=No"; }
```

## References
- Dark Patterns Quick Guide: [Confirmshaming](https://www.darkpatterns.org/types-of-dark-pattern#confirmshaming)
- Nielsen Norman Group: [Choice Architecture](https://www.nngroup.com/articles/decision-fatigue/)

</content>