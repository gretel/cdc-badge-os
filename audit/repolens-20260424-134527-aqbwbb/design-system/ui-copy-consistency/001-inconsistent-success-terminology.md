---
title: "[MEDIUM] Inconsistent success confirmation terminology"
severity: MEDIUM
domain: design-system/ui-copy-consistency
lens: ui-copy-consistency
labels:
  - "microcopy"
  - "terminology"
  - "i18n"
---

## Summary
The application uses inconsistent terminology for success confirmations across different UI contexts:

1. **Toast success messages** use `StringId::OK` (line 181, Fido2Ui.cpp; line 195, ExpertMenuUi.cpp):
   - `showToastSuccess(tr(StringId::OK), 2000);`

2. **System status strings** use `StringId::SAVED` for saved data (line 234, I18n.cpp):
   - `REG(SAVED, "Saved", "Gespeichert");`
   - `REG(DELETED, "Deleted", "Geloscht");`

3. **PIN change flow** uses "PIN changed" (line 249, I18n.cpp):
   - `REG(PIN_CHANGED, "PIN changed", "PIN geandert");`

4. **WiFi and NTP success messages** use "Connected!" and "Time synced!" (lines 286-287, I18n.cpp):
   - `REG(WIFI_CONNECTED, "Connected!", "Verbunden!");`
   - `REG(NTP_SUCCESS, "Time synced!", "Zeit synchronisiert!");`

This creates terminology drift where similar "success" states are communicated differently.

## Impact
- Users may be uncertain whether different success messages mean the same thing
- Inconsistency reduces polish and professional feel of the interface
- Makes it harder to establish a consistent voice/tone for the application
- Complicates translation workflow when similar concepts use different source strings

## Evidence
**File: `components/mod_fido2/src/Fido2Ui.cpp` line 181**
```cpp
ui::showToastSuccess(ui::tr(ui::StringId::OK), 2000);
```

**File: `components/cdc_os_ui/src/ExpertMenuUi.cpp` lines 181, 195**
```cpp
showToastSuccess(tr(StringId::OK));
```

**File: `components/cdc_ui/src/I18n.cpp` lines 234-235**
```cpp
REG(SAVED,          "Saved",            "Gespeichert");
REG(DELETED,        "Deleted",          "Geloscht");
```

**File: `components/cdc_ui/src/I18n.cpp` line 249**
```cpp
REG(PIN_CHANGED,    "PIN changed",      "PIN geandert");
```

## Recommended Fix
Establish a consistent success message pattern:

1. **Option A (Recommended):** Create specific success strings for toast notifications:
   - Add `StringId::SUCCESS` with value "Success" / "Erfolg"
   - Replace `tr(StringId::OK)` in toast calls with `tr(StringId::SUCCESS)`

2. **Option B:** Use existing `StringId::SAVED` where appropriate and reserve `OK` for button labels only

3. Document the convention in a microcopy guide:
   - Toast success messages: Use "Success" or specific action name
   - Status updates: Use "Saved", "Connected", "Changed" as appropriate
   - Button labels: Use "OK", "Apply", "Confirm"

## References
- UI Copy Consistency Guidelines: Button and Action Label Inconsistency
- Terminology Drift Across Features
