---
title: "[MEDIUM] Inconsistent action confirmation patterns (OK vs specific verbs)"
severity: MEDIUM
domain: design-system/ui-copy-consistency
lens: ui-copy-consistency
labels:
  - "microcopy"
  - "action-labels"
  - "confirmation"
---

## Summary
The application uses inconsistent patterns for action confirmation, mixing generic "OK" with specific action verbs:

1. **Generic "OK" used for varied actions:**
   - FIDO2: `showToastSuccess(tr(StringId::OK))` (Fido2Ui.cpp line 306)
   - Expert Menu: `showToastSuccess(tr(StringId::OK))` (ExpertMenuUi.cpp lines 181, 195)
   - GPG: `showToastSuccess(tr(StringId::OK))` (GpgModule.cpp lines 490, 520)
   - HID: `showToastSuccess(tr(StringId::OK))` (HidModule.cpp line 288)
   - TOTP: `showToastSuccess(tr(StringId::OK))` (TotpModule.cpp line 905)

2. **Module-specific success strings used:**
   - Password module: `showToastSuccess(mstr(STR_SAVED))` - "Saved" (PasswordModule.cpp line 528)
   - Password module: `showToastSuccess(mstr(STR_DELETED))` - "Deleted" (PasswordModule.cpp line 684)
   - NVS edit: `showToastInfo("Deleted")` (NvsEditModule.cpp lines 310, 337)

3. **Inconsistent use of StringId::OK vs specific action:**
   - Same "OK" string used for: PIN change, cache rebuild, cleanup, GPG operations, FIDO2 operations
   - Different modules use different patterns for similar operations

4. **Footer hints use "OK" inconsistently:**
   - `[Y] OK [N] Back` (generic confirmation)
   - `[Y] Approve  [N] Deny` (FIDO2-specific)
   - `[Y] Save` (brightness slider)
   - `[Y] OK` (PIN input, T9 input, date/time input)
   - `[Y] Type  [2/8] Scroll  [N] Back` (password module)

## Impact
- **Unclear action semantics**: "OK" doesn't communicate what action was taken
- **Inconsistent feedback**: Similar operations use different confirmation language
- **User confusion**: Harder to understand what just completed successfully
- **Reduced clarity**: "Saved" is clearer than "OK" for save operations
- **Pattern drift**: New developers won't know which pattern to follow

## Evidence
**Generic "OK" used across multiple modules:**

**File: `components/mod_fido2/src/Fido2Ui.cpp` line 306**
```cpp
ui::showToastSuccess(ui::tr(ui::StringId::OK), 2000);
```

**File: `components/cdc_os_ui/src/ExpertMenuUi.cpp` lines 181, 195**
```cpp
showToastSuccess(tr(StringId::OK));
```

**File: `components/mod_gpg/src/GpgModule.cpp` lines 490, 520**
```cpp
ui::showToastSuccess(ui::tr(ui::StringId::OK));
```

**File: `components/mod_hid/src/HidModule.cpp` line 288**
```cpp
ui::showToastSuccess(ui::tr(ui::StringId::OK));
```

**File: `components/mod_totp/src/TotpModule.cpp` line 905**
```cpp
ui::showToastSuccess(ui::tr(ui::StringId::OK));
```

**Module-specific success strings:**

**File: `components/mod_password/src/PasswordModule.cpp` lines 528, 684**
```cpp
ui::showToastSuccess(mstr(STR_SAVED));  // "Saved"
ui::showToastSuccess(mstr(STR_DELETED)); // "Deleted"
```

**File: `components/cdc_os_ui/src/ExpertMenuUi.cpp` line 47**
```cpp
showToastSuccess("OK", TOAST_DURATION_SHORT_MS);  // Hardcoded!
```

**Footer hint inconsistency:**

**File: `components/cdc_ui/src/I18n.cpp` lines 354-357**
```cpp
REG(HINT_OK_BACK,       "[Y] OK [N] Back",      "[Y] OK [N] Zuruck");
REG(HINT_APPROVE_DENY,  "[Y] Approve  [N] Deny","[Y] OK  [N] Abbruch");
REG(HINT_BRIGHTNESS,    "<4 6> Adjust [Y] Save", "<4 6> Anpassen [Y] Speichern");
REG(HINT_PIN_INPUT,     "[0-9] Input [Y] OK",   "[0-9] Eingabe [Y] OK");
```

## Recommended Fix
1. **Establish clear pattern for success confirmations:**
   - Use **specific action verbs** for toast messages
   - Reserve "OK" for **button labels only**, not status messages

2. **Add specific success strings to I18n:**
   ```cpp
   // In I18n.h
   CONFIRM_PRESSED,      // "Pressed" / "Gedrueckt"
   CONFIRM_APPROVED,     // "Approved" / "Bestaetigt"
   CONFIRM_APPLIED,      // "Applied" / "Angewendet"
   CONFIRM_GENERATED,    // "Generated" / "Generiert"
   ```

3. **Update module-specific patterns:**
   - FIDO2 approval: `showToastSuccess(tr(StringId::CONFIRM_APPROVED))`
   - GPG operations: Use specific action (e.g., "Key generated", "Sign completed")
   - HID advertising: `showToastSuccess(tr(StringId::ADVERTISING_STARTED))`
   - TOTP code: `showToastSuccess(tr(StringId::TYPED))` (already exists in module)
   - Expert menu cache: `showToastSuccess(tr(StringId::REBUILT))` or "Cache rebuilt"

4. **Standardize footer hints:**
   - Use action-specific labels: `[Y] Confirm`, `[Y] Save`, `[Y] Apply`
   - Reserve "OK" for generic dismissals only
   - Ensure German translations match the specificity

5. **Fix hardcoded "OK" in ExpertMenuUi:**
   ```cpp
   // Change from:
   showToastSuccess("OK", TOAST_DURATION_SHORT_MS);
   // To:
   showToastSuccess(tr(StringId::OK), TOAST_DURATION_SHORT_MS);
   ```

## References
- Button and Action Label Inconsistency: Primary action labels
- UI Copy Consistency: Confirmation patterns
- Microcopy: Specific vs generic feedback
