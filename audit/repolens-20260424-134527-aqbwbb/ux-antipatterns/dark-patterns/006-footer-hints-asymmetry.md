---
title: "[LOW] Multiple footer hints use asymmetric action labels across views"
severity: LOW
domain: ui
lens: dark-patterns
labels:
  - "audit:ux-antipatterns/dark-patterns"
---

## Summary
Multiple footer hints in the I18n system use asymmetric action labels where the primary action (Y) is more positive/action-oriented than the secondary action (N). This creates a subtle visual hierarchy bias toward the confirm/selct action.

**Evidence:**

File: `components/cdc_ui/src/I18n.cpp`, lines 352-363

```cpp
REG(HINT_OK_BACK,       "[Y] OK [N] Back",      "[Y] OK [N] Zuruck");
REG(HINT_APPROVE_DENY,  "[Y] Approve  [N] Deny","[Y] OK  [N] Abbruch");
REG(HINT_SELECT,        "[Y] Select",           "[Y] Auswahlen");
REG(HINT_BACK,          "[N] Back",             "[N] Zuruck");
```

**Asymmetries identified:**

1. **HINT_OK_BACK**: "OK" (positive confirmation) vs "Back" (neutral/positional)
2. **HINT_APPROVE_DENY**: "Approve/Deny" (formal, weighted) vs "OK/Abbruch" (neutral)
3. **HINT_SELECT**: Only defines Y action, no N alternative shown
4. **HINT_BACK**: Only defines N action, no Y alternative shown

These hints are used by:
- `ListView` (HINT_OK_BACK) - components/cdc_views/src/ListView.cpp:178
- `ConfirmView` (custom "Y=Ja N=Nein") - components/cdc_views/src/ConfirmView.cpp:185
- `InfoView` (HINT_SCROLL_BACK) - components/cdc_views/src/InfoView.cpp:142

## Impact

1. **Visual hierarchy bias**: The primary action (Y key) consistently uses more positive/action-oriented language than the secondary action (N key)
2. **Subtle nudging**: Users may feel slightly more encouraged to select/confirm than to navigate back/decline
3. **Inconsistency across views**: Different views use different patterns (ConfirmView uses "Ja/Nein", ListView uses "OK/Zuruck")
4. **Language imbalance**: English and German translations have different semantic weights (e.g., "Approve/Deny" vs "OK/Abbruch")

## Recommended Fix

Standardize on symmetric, neutral labels across all footer hints:

```cpp
// In components/cdc_ui/src/I18n.cpp, lines 352-363

// Use consistent Yes/No pattern for all binary choices
REG(HINT_OK_BACK,       "[Y] Yes  [N] No",       "[Y] Ja   [N] Nein");
REG(HINT_APPROVE_DENY,  "[Y] Yes  [N] No",       "[Y] Ja   [N] Nein");
REG(HINT_SELECT,        "[Y] Yes  [N] No",       "[Y] Ja   [N] Nein");
REG(HINT_BACK,          "[Y] Yes  [N] Back",     "[Y] Ja   [N] Zuruck");

// Or use action-oriented but balanced labels
REG(HINT_OK_BACK,       "[Y] Select  [N] Back",  "[Y] Auswahlen  [N] Zuruck");
REG(HINT_APPROVE_DENY,  "[Y] Confirm  [N] Cancel","[Y] Bestaetigen  [N] Abbrechen");
```

Ensure `ConfirmView` implementation matches the header declaration:
- Current implementation: `"Y=Ja  N=Nein"` (line 185 in ConfirmView.cpp)
- Header declaration: `"Y=OK  N=Abbruch"` (line 57 in ConfirmView.h)
- Choose one and update both files consistently

## References

- Dark Patterns Quick Guide: [Visual Hierarchy Manipulation](https://www.darkpatterns.org/types-of-dark-pattern#visual-hierarchy-manipulation)
- Nielsen Norman Group: [Button Labels](https://www.nngroup.com/articles/button-labels/) - consistency and clarity
- Material Design: [Buttons](https://material.io/components/buttons) - action-oriented but balanced labels
- WCAG 2.1: [Consistent Help](https://www.w3.org/WAI/WCAG21/Understanding/consistent-help.html) - consistent terminology across languages

</content>