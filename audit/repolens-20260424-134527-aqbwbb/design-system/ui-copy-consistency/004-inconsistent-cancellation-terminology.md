---
title: "[MEDIUM] Inconsistent cancellation/dismiss terminology"
severity: MEDIUM
domain: design-system/ui-copy-consistency
lens: ui-copy-consistency
labels:
  - "microcopy"
  - "action-labels"
  - "terminology"
---

## Summary
The application uses inconsistent terminology for cancellation and dismissal actions:

1. **Button labels:**
   - `StringId::CANCEL` = "Cancel" / "Abbrechen" (line 222, I18n.cpp)
   - `StringId::BACK` = "Back" / "Zuruck" (line 220, I18n.cpp)

2. **Footer hints use mixed patterns:**
   - "[N] Back" (line 351, I18n.cpp)
   - "[Y] OK [N] Back" (line 352, I18n.cpp)
   - "[Y] Approve  [N] Deny" (line 353, I18n.cpp)
   - "[Y] OK [N] Back" (DateInputView, TimeInputView)

3. **Confirmation dialogs use different patterns:**
   - FIDO2 UI: "Approve" / "Deny" (HINT_APPROVE_DENY)
   - Other contexts: "OK" / "Back"
   - ConfirmView: "Y=Ja  N=Nein" hardcoded (line 186, ConfirmView.cpp)

4. **Hardcoded dismiss text in ConfirmView:**
   - `"Y=Ja  N=Nein"` (line 186, ConfirmView.cpp) - not using i18n

## Impact
- Users encounter different terminology for similar dismiss actions
- Hardcoded German in ConfirmView breaks consistency when language changes
- "Back", "Cancel", "Deny" all serve similar purposes but feel different
- Inconsistent footer hints create cognitive load

## Evidence
**File: `components/cdc_ui/src/I18n.cpp` lines 220-222, 351-353**
```cpp
REG(BACK,           "Back",             "Zuruck");
REG(CANCEL,         "Cancel",           "Abbrechen");
REG(OK,             "OK",               "OK");
REG(HINT_OK_BACK,       "[Y] OK [N] Back",      "[Y] OK [N] Zuruck");
REG(HINT_APPROVE_DENY,  "[Y] Approve  [N] Deny","[Y] OK  [N] Abbruch");
```

**File: `components/cdc_views/src/ConfirmView.cpp` line 186**
```cpp
gfx->print("Y=Ja  N=Nein");  // Hardcoded German!
```

**File: `components/cdc_os_ui/src/Fido2Ui.cpp` (uses HINT_APPROVE_DENY)**
```cpp
ui::tr(ui::StringId::HINT_APPROVE_DENY)
```

## Recommended Fix
1. **Standardize cancellation terminology:**
   - Use "Cancel" for modal dismissals
   - Use "Back" for navigation
   - Use "Deny" only for approval/rejection contexts (FIDO2)

2. **Fix hardcoded German in ConfirmView:**
   - Replace `"Y=Ja  N=Nein"` with i18n string
   - Add new string IDs: `CONFIRM_YES_NO_EN`, `CONFIRM_YES_NO_DE`

3. **Consolidate footer hint patterns:**
   - Create consistent patterns: "[Y] Confirm [N] Cancel"
   - Avoid mixing "OK", "Back", "Deny" in similar contexts

4. **Document the convention:**
   - "Cancel" = dismiss without saving
   - "Back" = navigate to previous screen
   - "Deny" = reject approval request
   - "OK" = confirm/accept

## References
- Button and Action Label Inconsistency: Cancel/dismiss actions
- Hardcoded Strings vs Centralized Copy
