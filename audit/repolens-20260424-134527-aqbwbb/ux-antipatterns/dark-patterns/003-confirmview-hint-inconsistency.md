---
title: "[LOW] ConfirmView has inconsistent footer hints between header and implementation"
severity: LOW
domain: ui
lens: dark-patterns
labels:
  - "audit:ux-antipatterns/dark-patterns"
---

## Summary
The `ConfirmView` class has a mismatch between its declared footer hint in the header file and the actual rendered text in the implementation. This creates visual inconsistency and potential confusion about button labels.

**Evidence:**
- File: `components/cdc_views/include/cdc_views/ConfirmView.h`, line 57
- Declaration:
```cpp
const char* getFooterHint() const override { return "Y=OK  N=Abbruch"; }
```

- File: `components/cdc_views/src/ConfirmView.cpp`, line 185
- Actual rendered text:
```cpp
gfx->print("Y=Ja  N=Nein");  // German footer hint
```

The header declares `"Y=OK  N=Abbruch"` (OK/Abort) while the implementation renders `"Y=Ja  N=Nein"` (Yes/No).

## Impact
1. **Visual inconsistency**: The footer hint shown to users differs from what the `getFooterHint()` method promises
2. **Translation mismatch**: The English hint in the header says "OK/Abbruch" while German shows "Ja/Nein" - different semantic meanings
3. **Maintenance burden**: Developers relying on `getFooterHint()` for UI consistency will be misled
4. **Minor dark pattern**: The inconsistency itself is a form of misalignment between expected and actual behavior

## Recommended Fix
Choose one consistent set of labels and update both header and implementation:

**Option A - Use Yes/No (simpler, more universal):**
```cpp
// In components/cdc_views/include/cdc_views/ConfirmView.h, line 57
const char* getFooterHint() const override { return "Y=Yes  N=No"; }

// In components/cdc_views/src/ConfirmView.cpp, line 185
gfx->print("Y=Ja  N=Nein");  // Keep German, update English header
```

**Option B - Use OK/Cancel (more formal):**
```cpp
// In components/cdc_views/include/cdc_views/ConfirmView.h, line 57
const char* getFooterHint() const override { return "Y=OK  N=Cancel"; }

// In components/cdc_views/src/ConfirmView.cpp, line 185
gfx->print("Y=OK  N=Cancel");  // Update to match header
```

The implementation should be updated to use the I18n system for proper localization instead of hardcoded strings.

## References
- Dark Patterns Quick Guide: [Misdirection](https://www.darkpatterns.org/types-of-dark-pattern#misdirection)
- Material Design: [Buttons](https://material.io/components/buttons) - recommends consistent, clear action labels
- Nielsen Norman Group: [Labeling Buttons](https://www.nngroup.com/articles/button-labels/) - consistency in action labels
