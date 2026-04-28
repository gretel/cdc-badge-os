---
title: "[HIGH] Hardcoded confirmation dialog text (not using i18n)"
severity: HIGH
domain: design-system/ui-copy-consistency
lens: ui-copy-consistency
labels:
  - "microcopy"
  - "i18n"
  - "hardcoded-strings"
---

## Summary
The ConfirmView component has hardcoded German text that bypasses the i18n system:

**File: `components/cdc_views/src/ConfirmView.cpp` line 186**
```cpp
gfx->print("Y=Ja  N=Nein");  // Hardcoded German!
```

This means:
1. The confirmation hint is **always in German** regardless of the selected language
2. When user selects English, all other UI elements switch but this hint remains "Y=Ja  N=Nein"
3. The text is duplicated (once in code, once in I18n for German) but not connected

## Impact
- **Critical consistency issue**: Language switch doesn't apply to all UI elements
- German-speaking users see consistent text, but English users get a mix
- Breaks the i18n pattern established throughout the rest of the codebase
- Makes the application feel unpolished and incomplete
- Easy to fix but easy to miss during code review

## Evidence
**File: `components/cdc_views/src/ConfirmView.cpp` line 186**
```cpp
// Draw Y/N hint at bottom
gfx->setCursor(boxX + BOX_WIDTH / 2 - 30, boxY + BOX_HEIGHT - 12);
gfx->print("Y=Ja  N=Nein");  // <-- HARDCODED GERMAN!
```

**File: `components/cdc_ui/src/I18n.cpp` lines 220-221**
```cpp
REG(OK,             "OK",               "OK");
REG(CANCEL,         "Cancel",           "Abbrechen");
```
Note: There's no corresponding StringId for "Y/N confirmation hint"

## Recommended Fix
1. **Add new StringIds to I18n.h:**
   ```cpp
   // === Confirmation Hints ===
   HINT_CONFIRM_YES_NO,  // "Y=Yes  N=No" / "Y=Ja  N=Nein"
   ```

2. **Add translations to I18n.cpp:**
   ```cpp
   REG(HINT_CONFIRM_YES_NO, "Y=Yes  N=No", "Y=Ja  N=Nein");
   ```

3. **Update ConfirmView.cpp:**
   ```cpp
   gfx->setCursor(boxX + BOX_WIDTH / 2 - 30, boxY + BOX_HEIGHT - 12);
   gfx->print(tr(StringId::HINT_CONFIRM_YES_NO));
   ```

4. **Audit for similar hardcoded strings:**
   - Search for `gfx->print("...")` patterns in view files
   - Check for other hardcoded UI text

## References
- Hardcoded Strings vs Centralized Copy guidelines
- Internationalization best practices
- UI Copy Consistency: Centralized copy management
