---
title: "[LOW] ListView uses asymmetric OK/Back button labels"
severity: LOW
domain: ui
lens: dark-patterns
labels:
  - "audit:ux-antipatterns/dark-patterns"
---

## Summary
The `ListView` component uses `HINT_OK_BACK` which has asymmetric button labels: "OK" (positive, action-oriented) vs "Back" (neutral, positional).

**Evidence:**
- File: `components/cdc_views/src/ListView.cpp`
- Code:
```cpp
return tr(StringId::HINT_OK_BACK);
```

- File: `components/cdc_ui/src/I18n.cpp`
- Code:
```cpp
REG(HINT_OK_BACK,       "[Y] OK [N] Back",      "[Y] OK [N] Zuruck");
```

The English label "OK/Back" and German "OK/Zuruck" both have the same asymmetry.

## Impact
1. **Subtle nudge**: "OK" is more positive/action-oriented than "Back", which could subtly encourage users to select rather than return
2. **Visual hierarchy**: The primary action (Y/Select) is framed as "OK" while the secondary action (N) is "Back"
3. **Consistency issue**: Different views use different label patterns (ConfirmView uses "Ja/Nein", ListView uses "OK/Zuruck")

## Recommended Fix
Use symmetric, neutral labels:

```cpp
// In components/cdc_ui/src/I18n.cpp
REG(HINT_OK_BACK,       "[Y] Select  [N] Back",  "[Y] Auswahlen  [N] Zuruck");
```

Or use consistent Yes/No pattern:
```cpp
REG(HINT_OK_BACK,       "[Y] Yes  [N] No",       "[Y] Ja  [N] Nein");
```

## References
- Dark Patterns Quick Guide: [Visual Hierarchy Manipulation](https://www.darkpatterns.org/types-of-dark-pattern#visual-hierarchy-manipulation)
- Nielsen Norman Group: [Button Labels](https://www.nngroup.com/articles/button-labels/) - consistency and clarity
- Material Design: [Buttons](https://material.io/components/buttons) - action-oriented but balanced labels
