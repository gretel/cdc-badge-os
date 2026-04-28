---
title: "[MEDIUM] Confusing confirmation dialog patterns (Y/N vs specific actions)"
severity: MEDIUM
domain: design-system/ui-copy-consistency
lens: ui-copy-consistency
labels:
  - "microcopy"
  - "confirmation-dialogs"
  - "action-labels"
---

## Summary
Confirmation dialogs use inconsistent and sometimes confusing patterns for confirm/cancel actions:

1. **Generic Y/N with hardcoded text:**
   - ConfirmView uses hardcoded `"Y=Ja  N=Nein"` (line 186) - not using i18n
   - Same dialog used for all confirmations regardless of action

2. **Footer hints mix generic and specific:**
   - `[Y] OK [N] Back` - generic
   - `[Y] Approve  [N] Deny` - FIDO2-specific
   - `[Y] Select` - list navigation
   - `[Y] Save` - brightness slider
   - `[Y] OK` - input confirmation

3. **Confirmation text lacks consequence description:**
   - Password module: "Delete entry?" (STR_CONFIRM_DELETE) - good, specific
   - ConfirmView: Generic Y/N hint without context
   - No pattern for showing what will happen after confirmation

4. **Button labels don't match action:**
   - Delete confirmation shows `[Y] OK [N] Back` instead of `[Y] Delete [N] Keep`
   - Generic "OK" doesn't communicate destructive nature

5. **German translation of HINT_APPROVE_DENY is inconsistent:**
   - English: `[Y] Approve  [N] Deny`
   - German: `[Y] OK  [N] Abbruch` (uses "OK" instead of "Approve")

## Impact
- **User confusion**: Generic "OK" for delete doesn't warn of action
- **Accidental data loss**: Users may press Y without understanding consequence
- **Inconsistent mental model**: Different dialogs feel different
- **Translation drift**: German "OK" vs English "Approve" breaks symmetry

## Evidence
**File: `components/cdc_views/src/ConfirmView.cpp` line 186**
```cpp
gfx->print("Y=Ja  N=Nein");  // Hardcoded German!
```

**File: `components/cdc_ui/src/I18n.cpp` line 355**
```cpp
REG(HINT_APPROVE_DENY,  "[Y] Approve  [N] Deny","[Y] OK  [N] Abbruch");
// English: Approve/Deny
// German:  OK/Abbruch (mismatch!)
```

**File: `components/mod_password/src/PasswordModule.cpp` line 90**
```cpp
i18n.registerTranslation(s_strIdBase + STR_CONFIRM_DELETE, ui::Language::EN, "Delete entry?");
i18n.registerTranslation(s_strIdBase + STR_CONFIRM_DELETE, ui::Language::DE, "Eintrag loeschen?");
```
This is good - specific confirmation text!

**File: `components/cdc_ui/src/I18n.cpp` lines 354, 356-357**
```cpp
REG(HINT_OK_BACK,       "[Y] OK [N] Back",      "[Y] OK [N] Zuruck");
REG(HINT_BRIGHTNESS,    "<4 6> Adjust [Y] Save", "<4 6> Anpassen [Y] Save");
REG(HINT_PIN_INPUT,     "[0-9] Input [Y] OK",   "[0-9] Eingabe [Y] OK");
```

**File: `components/cdc_os_ui/src/ExpertMenuUi.cpp` (uses ConfirmView)**
```cpp
// Uses generic Y/N confirmation for cache rebuild, cleanup, etc.
```

## Recommended Fix
1. **Fix hardcoded German in ConfirmView:**
   ```cpp
   // Add to I18n.h
   HINT_CONFIRM_YES_NO,  // "Y=Yes  N=No" / "Y=Ja  N=Nein"
   
   // Add to I18n.cpp
   REG(HINT_CONFIRM_YES_NO, "Y=Yes  N=No", "Y=Ja  N=Nein");
   
   // Update ConfirmView.cpp
   gfx->print(ui::tr(ui::StringId::HINT_CONFIRM_YES_NO));
   ```

2. **Fix German translation mismatch:**
   ```cpp
   // In I18n.cpp
   REG(HINT_APPROVE_DENY,  "[Y] Approve  [N] Deny", "[Y] Bestaetigen  [N] Ablehnen");
   // Now both languages use action-specific terms
   ```

3. **Add action-specific confirmation hints:**
   ```cpp
   // In I18n.h
   HINT_DELETE_CONFIRM,    // "Y=Delete  N=Keep" / "Y=Loeschen  N=Behalten"
   HINT_SAVE_CONFIRM,      // "Y=Save  N=Cancel" / "Y=Speichern  N=Abbrechen"
   
   // In I18n.cpp
   REG(HINT_DELETE_CONFIRM, "Y=Delete  N=Keep", "Y=Loeschen  N=Behalten");
   REG(HINT_SAVE_CONFIRM,   "Y=Save  N=Cancel", "Y=Speichern  N=Abbrechen");
   ```

4. **Pass context to confirmation dialogs:**
   - Modify ConfirmView to accept hint text parameter
   - Use action-specific hints based on operation
   - Example: Delete uses "Y=Delete  N=Keep", Save uses "Y=Save  N=Cancel"

5. **Document confirmation pattern:**
   - Destructive actions: Use specific verbs (Delete, Remove, Clear)
   - Non-destructive: Use generic (OK, Confirm, Save)
   - Always show consequence in message body

## References
- Confirmation Dialog Copy guidelines
- Button and Action Label Inconsistency: Destructive actions
- UI Copy Consistency: Specific vs generic action labels
