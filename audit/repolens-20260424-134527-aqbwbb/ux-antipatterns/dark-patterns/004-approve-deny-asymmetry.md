---
title: "[LOW] HINT_APPROVE_DENY uses asymmetric labels between English and German"
severity: LOW
domain: ui
lens: dark-patterns
labels:
  - "audit:ux-antipatterns/dark-patterns"
---

## Summary
The `HINT_APPROVE_DENY` string has asymmetric labels between English and German translations. English uses "Approve/Deny" while German uses "OK/Abbruch" - different semantic weight and formality levels.

**Evidence:**
- File: `components/cdc_ui/src/I18n.cpp`
- Code:
```cpp
REG(HINT_APPROVE_DENY,  "[Y] Approve  [N] Deny","[Y] OK  [N] Abbruch");
```

The English version uses formal "Approve/Deny" while German uses neutral "OK/Abort".

## Impact
1. **Asymmetric tone**: English "Approve/Deny" has more formal weight than German "OK/Abbruch"
2. **Inconsistent UX**: Users get different psychological framing depending on language
3. **Potential confirmshaming**: "Deny" is slightly more negative than "Abbruch" (which is neutral "Abort")
4. **FIDO2 approval context**: This hint is used in FIDO2 authentication flows where clear, neutral language is critical

## Recommended Fix
Use consistent, neutral labels in both languages:

```cpp
// In components/cdc_ui/src/I18n.cpp
REG(HINT_APPROVE_DENY,  "[Y] Yes  [N] No", "[Y] Ja  [N] Nein");
```

Or use action-oriented labels:
```cpp
REG(HINT_APPROVE_DENY,  "[Y] Confirm  [N] Cancel", "[Y] Bestaetigen  [N] Abbrechen");
```

## References
- Dark Patterns Quick Guide: [Confirmshaming](https://www.darkpatterns.org/types-of-dark-pattern#confirmshaming)
- Nielsen Norman Group: [Button Labels](https://www.nngroup.com/articles/button-labels/) - consistency across languages
- WCAG 2.1: [Consistent Help](https://www.w3.org/WAI/WCAG21/Understanding/consistent-help.html) - consistent terminology
